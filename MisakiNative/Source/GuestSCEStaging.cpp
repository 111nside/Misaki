#include "../Include/GuestSCEStaging.hpp"
#include <algorithm>
#include <limits>
#include <set>
#include <utility>

namespace misaki { namespace {
void fail(std::string *e, const char *message) { if (e) *e = message; }
std::uint64_t get(const std::uint8_t *b, std::size_t p, unsigned n) {
    std::uint64_t v = 0;
    for (unsigned i = 0; i < n; ++i) v |= std::uint64_t(b[p+i]) << (i*8);
    return v;
}
void put(std::vector<std::uint8_t> &b, std::size_t p, std::uint64_t v, unsigned n) {
    for (unsigned i=0; i<n; ++i) b[p+i] = std::uint8_t(v >> (i*8));
}
bool within(std::uint64_t p, std::uint64_t n, std::size_t size) {
    return p <= size && n <= size - p;
}
bool sum(std::uint64_t a, std::uint64_t b, std::uint64_t &result) {
    if (b > UINT64_MAX - a) return false;
    result = a + b;
    return true;
}
struct Segment { std::uint64_t address, start, filesz, memsz; std::uint8_t flags; };
} // namespace

std::optional<SCEStagedImage> stageSCEELFFixture(
    const std::uint8_t *bytes, std::size_t length, std::uint64_t bias,
    const std::vector<SCERelativePatch> &patches, GuestMemory &memory,
    std::string *error) {
    if (error) error->clear();
    if (!bytes || length < 120 || length > 8*1024*1024 || patches.size() > 64) {
        fail(error, "Invalid staging input or relocation budget"); return std::nullopt;
    }
    const auto metadata = inspectPS4Executable(bytes, length, error);
    // Encrypted or unencrypted SELF containers are ALWAYS rejected here.
    if (!metadata || metadata->kind != PS4ContainerKind::sceELF ||
        metadata->elfType != 0xFE10 || metadata->programHeaders == 0 ||
        metadata->programHeaders > 16) {
        fail(error, "Only unwrapped, self-authored SCE_DYNEXEC ELF fixtures can be staged");
        return std::nullopt;
    }
    const auto phoff = get(bytes, 32, 8);
    std::vector<Segment> segments;
    std::uint64_t loadedBytes = 0;
    for (std::uint32_t i = 0; i < metadata->programHeaders; ++i) {
        const auto p = phoff + i * 56;
        if (!within(p, 56, length)) { fail(error,"Invalid program header"); return std::nullopt; }
        if (get(bytes, p, 4) != 1) { fail(error,"Only PT_LOAD segments are supported"); return std::nullopt; }
        const auto pf = get(bytes, p+4, 4);
        const auto offset = get(bytes, p+8, 8);
        const auto vaddr = get(bytes, p+16, 8);
        const auto filesz = get(bytes, p+32, 8);
        const auto memsz = get(bytes, p+40, 8);
        std::uint64_t addr = 0;
        if ((pf & ~7ULL) || ((pf & 2) && (pf & 1)) ||
            memsz == 0 || memsz > 1024*1024 || filesz > memsz ||
            loadedBytes > 1024*1024 - memsz || !within(offset, filesz, length) ||
            !sum(vaddr, bias, addr) || addr > UINT64_MAX - memsz) {
            fail(error, "Unsupported segment permissions, range or memory budget");
            return std::nullopt;
        }
        loadedBytes += memsz;
        const auto flags = static_cast<std::uint8_t>(
            ((pf & 4) ? permission::read : 0) |
            ((pf & 2) ? permission::write : 0) |
            ((pf & 1) ? permission::execute : 0));
        segments.push_back({addr, offset, filesz, memsz, flags});
    }
    std::uint64_t entry = 0;
    if (!sum(metadata->entry, bias, entry)) { fail(error,"Entry address overflow"); return std::nullopt; }
    GuestMemory scratch = memory;
    for (const auto &s : segments) {
        if (!scratch.map(s.address, static_cast<std::size_t>(s.memsz),
                         permission::read | permission::write) ||
            (s.filesz && !scratch.writeBytes(s.address, bytes + s.start,
                                             static_cast<std::size_t>(s.filesz)))) {
            fail(error,"Segment mapping overlaps existing memory or payload not writable");
            return std::nullopt;
        }
    }
    std::set<std::uint64_t> seen;
    SCEStagedImage out;
    out.entry = entry;
    out.loadBias = bias;
    out.mappedSegments = static_cast<std::uint32_t>(segments.size());
    for (const auto &patch : patches) {
        std::uint64_t target = 0, value = 0;
        if (!sum(bias, patch.virtualAddress, target) ||
            !sum(bias, patch.addend, value) || !seen.insert(target).second) {
            fail(error,"Invalid or duplicate synthetic relative relocation"); return std::nullopt;
        }
        bool insideLoaded = false;
        for (const auto &s : segments)
            if (s.memsz >= 8 && target >= s.address && target - s.address <= s.memsz - 8)
                insideLoaded = true;
        if (!insideLoaded || !scratch.write64(target, value)) {
            fail(error,"Relative relocation outside loaded SCE image"); return std::nullopt;
        }
        ++out.relativeRelocations;
        out.lastRelocationValue = value;
    }
    for (const auto &s : segments)
        if (!scratch.protect(s.address, static_cast<std::size_t>(s.memsz), s.flags)) {
            fail(error,"Failed to apply final memory permissions"); return std::nullopt;
        }
    if (!scratch.isExecutable(entry)) { fail(error,"Entry does not map to executable code"); return std::nullopt; }
    memory = std::move(scratch);
    return out;
}

std::vector<std::uint8_t> makeSCEStagingFixture() {
    std::vector<std::uint8_t> bytes(0x280, 0);
    bytes[0]=0x7f; bytes[1]='E'; bytes[2]='L'; bytes[3]='F';
    bytes[4]=2; bytes[5]=1; bytes[6]=1;
    put(bytes,16,0xFE10,2); put(bytes,18,62,2); put(bytes,20,1,4);
    put(bytes,24,0x1000,8); put(bytes,32,64,8);
    put(bytes,52,64,2); put(bytes,54,56,2); put(bytes,56,1,2);
    put(bytes,64,1,4); put(bytes,68,5,4); // one RX PT_LOAD
    put(bytes,72,0x200,8); put(bytes,80,0x1000,8);
    put(bytes,96,0x80,8); put(bytes,104,0x100,8);
    put(bytes,112,0x1000,8);
    const std::uint8_t code[] = {0x48,0xB8,42,0,0,0,0,0,0,0,0xF4};
    std::copy(std::begin(code),std::end(code),bytes.begin()+0x200);
    return bytes;
}
} // namespace misaki
