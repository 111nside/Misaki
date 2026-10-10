#include "../Include/GuestDynamicExecution.hpp"

#include <algorithm>
#include <limits>
#include <iterator>
#include <set>
#include <string>
#include <utility>

namespace misaki {
namespace {
using u8 = std::uint8_t;
using u32 = std::uint32_t;
using u64 = std::uint64_t;
constexpr u64 maxFileSize = 16 * 1024 * 1024;
constexpr u64 maxDynamicEntries = 128;
constexpr u64 maxRelocations = 128;
constexpr u64 maxStringTable = 4096;
constexpr u64 maxSymbols = 256;

u32 read32(const u8 *bytes, std::size_t offset) {
    u32 result = 0;
    for (unsigned i = 0; i < 4; ++i) result |= u32(bytes[offset + i]) << (i * 8);
    return result;
}
u64 read64(const u8 *bytes, std::size_t offset) {
    u64 result = 0;
    for (unsigned i = 0; i < 8; ++i) result |= u64(bytes[offset + i]) << (i * 8);
    return result;
}
void put(std::vector<u8> &bytes, std::size_t offset, u64 value, unsigned width) {
    for (unsigned i = 0; i < width; ++i)
        bytes[offset + i] = static_cast<u8>(value >> (i * 8));
}
bool add(u64 lhs, u64 rhs, u64 &out) {
    if (rhs > std::numeric_limits<u64>::max() - lhs) return false;
    out = lhs + rhs;
    return true;
}
bool addSigned(u64 lhs, std::int64_t rhs, u64 &out) {
    if (rhs >= 0) return add(lhs, static_cast<u64>(rhs), out);
    const u64 magnitude = u64(0) - static_cast<u64>(rhs);
    if (lhs < magnitude) return false;
    out = lhs - magnitude;
    return true;
}

struct LoadSegment {
    u64 vaddr = 0;
    u64 fileOffset = 0;
    u64 fileSize = 0;
};

struct FileReader {
    const u8 *bytes;
    std::size_t size;
    std::vector<LoadSegment> segments;

    // Translate a guest virtual address into a file offset, only if all
    // requested bytes are part of the file-backed PT_LOAD region.
    std::optional<std::size_t> offset(u64 address, u64 length) const {
        if (length == 0 || length > size) return std::nullopt;
        u64 end = 0;
        if (!add(address, length, end)) return std::nullopt;
        for (const auto &s : segments) {
            u64 segmentEnd = 0;
            if (!add(s.vaddr, s.fileSize, segmentEnd)) return std::nullopt;
            if (address < s.vaddr || end > segmentEnd) continue;
            const auto relative = address - s.vaddr;
            u64 absolute = 0;
            if (!add(s.fileOffset, relative, absolute) ||
                absolute > size || length > size - absolute) return std::nullopt;
            return static_cast<std::size_t>(absolute);
        }
        return std::nullopt;
    }
};

std::optional<std::string> stringAt(const FileReader &file, u64 stringVaddr,
                                    u64 stringSize, u64 index) {
    if (index >= stringSize) return std::nullopt;
    u64 address = 0;
    if (!add(stringVaddr, index, address)) return std::nullopt;
    auto at = file.offset(address, stringSize - index);
    if (!at) return std::nullopt;
    const auto *start = file.bytes + *at;
    const auto *end = std::find(start, start + static_cast<std::size_t>(stringSize - index), 0);
    if (start == end || end == start + (stringSize - index)) return std::nullopt;
    return std::string(reinterpret_cast<const char *>(start),
                       reinterpret_cast<const char *>(end));
}

} // namespace

std::optional<DynamicLinkSummary> loadAndLinkDynamicELF(
    const u8 *elf, std::size_t length, u64 bias,
    const ModuleRegistry &modules, GuestMemory &memory) {
    if (!elf || length < 64 || length > maxFileSize) return std::nullopt;
    const auto info = inspectELF64(elf, length);
    if (!info || info->type != 3 || !info->hasDynamicSegment) return std::nullopt;
    const u64 phoff = read64(elf, 32);
    const auto phsize = static_cast<std::size_t>(elf[54] | (unsigned(elf[55]) << 8));
    const auto phcount = static_cast<std::size_t>(elf[56] | (unsigned(elf[57]) << 8));
    FileReader file{elf, length, {}};
    u64 dynamicOffset = 0;
    u64 dynamicVaddr = 0;
    u64 dynamicSize = 0;
    unsigned dynamicCount = 0;
    for (std::size_t i = 0; i < phcount; ++i) {
        const auto at = static_cast<std::size_t>(phoff) + i * phsize;
        const auto kind = read32(elf, at);
        if (kind == 1) {
            file.segments.push_back({read64(elf, at + 16), read64(elf, at + 8),
                                     read64(elf, at + 32)});
        } else if (kind == 2) {
            ++dynamicCount;
            dynamicOffset = read64(elf, at + 8);
            dynamicVaddr = read64(elf, at + 16);
            dynamicSize = read64(elf, at + 32);
        }
    }
    if (dynamicCount != 1 || dynamicSize < 16 || dynamicSize % 16 != 0 ||
        dynamicSize / 16 > maxDynamicEntries || dynamicOffset > length ||
        dynamicSize > length - dynamicOffset) return std::nullopt;
    const auto dynamicBackedOffset = file.offset(dynamicVaddr, dynamicSize);
    if (!dynamicBackedOffset || *dynamicBackedOffset != dynamicOffset)
        return std::nullopt;

    u64 stringTable = 0, symbolTable = 0, stringSize = 0, symbolSize = 0;
    u64 relaTable = 0, relaSize = 0, relaEntrySize = 0;
    std::vector<u64> neededOffsets;
    bool terminated = false;
    std::set<u64> seenTags;
    for (u64 i = 0; i < dynamicSize / 16; ++i) {
        const auto at = static_cast<std::size_t>(dynamicOffset + i * 16);
        const auto tag = read64(elf, at);
        const auto value = read64(elf, at + 8);
        if (tag == 0) { terminated = true; break; }
        if (tag == 1) {
            if (neededOffsets.size() >= 8) return std::nullopt;
            neededOffsets.push_back(value);
            continue;
        }
        if (tag == 5 || tag == 6 || tag == 10 || tag == 11 ||
            tag == 7 || tag == 8 || tag == 9) {
            if (!seenTags.insert(tag).second) return std::nullopt;
            switch (tag) {
            case 5: stringTable = value; break;
            case 6: symbolTable = value; break;
            case 10: stringSize = value; break;
            case 11: symbolSize = value; break;
            case 7: relaTable = value; break;
            case 8: relaSize = value; break;
            case 9: relaEntrySize = value; break;
            }
        } else {
            // Unsupported dynamic tags need deliberate support rather than
            // being silently ignored by an emulator prototype.
            return std::nullopt;
        }
    }
    if (!terminated || neededOffsets.empty() || stringTable == 0 || symbolTable == 0 ||
        stringSize == 0 || stringSize > maxStringTable || symbolSize != 24 ||
        relaTable == 0 || relaSize == 0 || relaEntrySize != 24 ||
        relaSize % relaEntrySize != 0 || relaSize / relaEntrySize > maxRelocations ||
        !file.offset(stringTable, stringSize) || !file.offset(relaTable, relaSize))
        return std::nullopt;

    std::vector<std::string> needed;
    for (const auto offset : neededOffsets) {
        auto name = stringAt(file, stringTable, stringSize, offset);
        if (!name || std::find(needed.begin(), needed.end(), *name) != needed.end())
            return std::nullopt;
        needed.push_back(*name);
    }

    // Every mutation below happens on a copy; the caller's memory is not
    // modified if a table is malformed, import is missing, or a write fails.
    GuestMemory candidate = memory;
    const auto image = loadGuestELF(elf, length, bias, candidate);
    if (!image) return std::nullopt;
    DynamicLinkSummary summary;
    summary.entry = image->entry;
    summary.mappedSegments = image->segments;
    summary.neededLibraries = static_cast<u32>(needed.size());
    // Record each byte interval, not only slot starts: overlapping mixed-width
    // RELA records are rejected instead of silently overwriting one another.
    std::vector<std::pair<u64, u64>> patchedRanges;
    for (u64 i = 0; i < relaSize / 24; ++i) {
        auto at = file.offset(relaTable + i * 24, 24);
        if (!at) return std::nullopt;
        const auto relocOffset = read64(elf, *at);
        const auto infoWord = read64(elf, *at + 8);
        const auto relocType = static_cast<u32>(infoWord);
        const auto symbolIndex = infoWord >> 32;
        const auto addend = static_cast<std::int64_t>(read64(elf, *at + 16));
        const bool relative = relocType == 8 && symbolIndex == 0;
        const bool legacyImport = (relocType == 6 || relocType == 7) &&
                                  symbolIndex > 0 && addend == 0;
        const bool extendedImport = (relocType == 1 || relocType == 2 ||
                                     relocType == 4 || relocType == 10 ||
                                     relocType == 11) && symbolIndex > 0;
        if (!relative && !legacyImport && !extendedImport) return std::nullopt;
        const u64 width = (relocType == 2 || relocType == 4 ||
                           relocType == 10 || relocType == 11) ? 4 : 8;
        u64 slot = 0, end = 0;
        if (!add(bias, relocOffset, slot) || !add(slot, width, end))
            return std::nullopt;
        for (const auto &used : patchedRanges)
            if (slot < used.second && used.first < end) return std::nullopt;
        patchedRanges.push_back({slot, end});

        u64 symbolAddress = 0;
        if (!relative) {
            if (symbolIndex >= maxSymbols) return std::nullopt;
            u64 symVaddr = 0;
            if (!add(symbolTable, symbolIndex * 24, symVaddr)) return std::nullopt;
            const auto symFile = file.offset(symVaddr, 24);
            if (!symFile) return std::nullopt;
            const auto nameIndex = read32(elf, *symFile);
            const auto sectionIndex = static_cast<unsigned>(elf[*symFile + 6]) |
                                      (unsigned(elf[*symFile + 7]) << 8);
            if (sectionIndex != 0) return std::nullopt; // undefined imports only
            auto symbol = stringAt(file, stringTable, stringSize, nameIndex);
            if (!symbol) return std::nullopt;
            bool found = false;
            for (const auto &lib : needed) {
                if (const auto target = modules.resolve(lib, *symbol)) {
                    if (found) return std::nullopt; // ambiguous dependency
                    symbolAddress = *target;
                    found = true;
                }
            }
            if (!found) return std::nullopt;
            if (summary.importedSymbols == 0) summary.firstImportTarget = symbolAddress;
            ++summary.importedSymbols;
        }
        u64 value = 0;
        if (relative) {
            if (!addSigned(bias, addend, value)) return std::nullopt;
            ++summary.relativeRelocations;
        } else if (legacyImport) {
            value = symbolAddress;
        } else if (relocType == 1 || relocType == 10 || relocType == 11) {
            // R_X86_64_64/32/32S: S + A, checked before truncation.
            if (!addSigned(symbolAddress, addend, value)) return std::nullopt;
            if (relocType == 10 && value > std::numeric_limits<u32>::max())
                return std::nullopt;
            if (relocType == 11) {
                // A valid 32S relocation fits after sign-extension.
                // Compare bit patterns to avoid signed out-of-range casts.
                if (value > u64(std::numeric_limits<std::int32_t>::max()) &&
                    value < std::numeric_limits<u64>::max() -
                                u64(std::numeric_limits<std::int32_t>::max()))
                    return std::nullopt;
            }
            ++summary.absoluteRelocations;
        } else { // R_X86_64_PC32 / PLT32: S + A - P, signed 32-bit
            u64 baseValue = 0;
            if (!addSigned(symbolAddress, addend, baseValue)) return std::nullopt;
            if (baseValue >= slot) {
                const u64 magnitude = baseValue - slot;
                if (magnitude > u64(std::numeric_limits<std::int32_t>::max()))
                    return std::nullopt;
                value = magnitude;
            } else {
                const u64 magnitude = slot - baseValue;
                if (magnitude > (u64(1) << 31)) return std::nullopt;
                value = u32(0) - static_cast<u32>(magnitude);
            }
            ++summary.pcRelativeRelocations;
        }
        if (width == 8) {
            if (!candidate.write64(slot, value)) return std::nullopt;
        } else {
            u8 field[4];
            for (unsigned k = 0; k < 4; ++k)
                field[k] = static_cast<u8>(value >> (8 * k));
            if (!candidate.writeBytes(slot, field, 4)) return std::nullopt;
        }
    }
    memory = std::move(candidate);
    return summary;
}

std::vector<u8> makeDynamicCPUFixture() {
    // Original ordinary ELF64 ET_DYN fixture, not a PS4 SELF/PRX.
    std::vector<u8> elf(0x440, 0);
    elf[0] = 0x7f; elf[1] = 'E'; elf[2] = 'L'; elf[3] = 'F';
    elf[4] = 2; elf[5] = 1; elf[6] = 1;
    put(elf, 16, 3, 2); put(elf, 18, 0x3e, 2); put(elf, 20, 1, 4);
    put(elf, 24, 0x1000, 8); put(elf, 32, 64, 8);
    put(elf, 52, 64, 2); put(elf, 54, 56, 2); put(elf, 56, 3, 2);
    // PT_LOAD: RX code at virtual offset 0x1000.
    put(elf, 64, 1, 4); put(elf, 68, 5, 4);
    put(elf, 72, 0x100, 8); put(elf, 80, 0x1000, 8);
    put(elf, 96, 18, 8); put(elf, 104, 18, 8);
    // PT_LOAD: RW data, dynamic metadata, symbols, strings and RELA.
    put(elf, 120, 1, 4); put(elf, 124, 6, 4);
    put(elf, 128, 0x200, 8); put(elf, 136, 0x1100, 8);
    put(elf, 152, 0x240, 8); put(elf, 160, 0x240, 8);
    // PT_DYNAMIC: dynamic entries live inside the RW PT_LOAD segment.
    put(elf, 176, 2, 4); put(elf, 180, 6, 4);
    put(elf, 184, 0x240, 8); put(elf, 192, 0x1140, 8);
    put(elf, 208, 144, 8); put(elf, 216, 144, 8);

    const u8 code[] = {
        0x48, 0xb8, 40, 0, 0, 0, 0, 0, 0, 0, // MOV RAX,40
        0x48, 0xff, 0x15, 0xef, 0, 0, 0,    // CALL qword [RIP+0xef] => 0x1100
        0xf4                                // HLT
    };
    std::copy(std::begin(code), std::end(code), elf.begin() + 0x100);
    const std::string library = "libMisakiDynamic";
    const std::string symbol = "increment2";
    constexpr u64 nameIndex = 1 + sizeof("libMisakiDynamic");
    std::size_t position = 0x380;
    elf[position++] = 0;
    for (const char c : library) elf[position++] = static_cast<u8>(c);
    elf[position++] = 0;
    for (const char c : symbol) elf[position++] = static_cast<u8>(c);
    elf[position++] = 0;
    const u64 stringSize = position - 0x380;
    // Symbol table contains the standard null symbol followed by undefined
    // guest import symbol number one (24 bytes each).
    put(elf, 0x300 + 24, nameIndex, 4);
    // ELF64 RELA entries: one RELATIVE, one JUMP_SLOT (symbol index 1).
    put(elf, 0x400, 0x1108, 8);
    put(elf, 0x408, 8, 8);
    put(elf, 0x410, 0x1234, 8);
    put(elf, 0x418, 0x1100, 8);
    put(elf, 0x420, (u64(1) << 32) | 7, 8);
    put(elf, 0x428, 0, 8);
    // Nine dynamic descriptors including DT_NULL terminator.
    const std::pair<u64, u64> entries[] = {
        {1, 1}, {5, 0x1280}, {6, 0x1200}, {10, stringSize}, {11, 24},
        {7, 0x1300}, {8, 48}, {9, 24}, {0, 0}
    };
    for (unsigned i = 0; i < 9; ++i) {
        put(elf, 0x240 + i * 16, entries[i].first, 8);
        put(elf, 0x248 + i * 16, entries[i].second, 8);
    }
    return elf;
}

std::optional<DynamicExecutionDiagnostic> runDynamicCPUBackendDiagnostic() {
    auto elf = makeDynamicCPUFixture();
    GuestMemory memory;
    const u8 libraryCode[] = {0x48, 0x83, 0xc0, 0x02, 0xc3}; // ADD RAX,2; RET
    if (!memory.map(0x9000, sizeof(libraryCode), permission::read | permission::write) ||
        !memory.writeBytes(0x9000, libraryCode, sizeof(libraryCode)) ||
        !memory.protect(0x9000, sizeof(libraryCode), permission::read | permission::execute))
        return std::nullopt;
    ModuleRegistry registry;
    if (!registry.registerModule({"libMisakiDynamic", {{"increment2", 0x9000}}}))
        return std::nullopt;
    const auto linked = loadAndLinkDynamicELF(elf.data(), elf.size(), 0x4000,
                                              registry, memory);
    if (!linked || linked->importedSymbols != 1 || linked->relativeRelocations != 1)
        return std::nullopt;
    const auto import = memory.read64(0x5100);
    const auto relative = memory.read64(0x5108);
    if (!import || !relative) return std::nullopt;
    // Diagnostic-only: the fixture's data mapping has no mutable variables.
    // Do not globally make every ET_DYN data segment read-only in real programs.
    if (!memory.protect(0x5100, 0x240, permission::read)) return std::nullopt;
    const bool readOnly = !memory.write64(0x5100, 0);
    PortableX64Backend backend(std::move(memory), linked->entry);
    DynamicExecutionDiagnostic result;
    result.execution = backend.run(30);
    result.linked = *linked;
    result.relativeValue = *relative;
    result.importReadOnly = readOnly;
    return result;
}

} // namespace misaki
