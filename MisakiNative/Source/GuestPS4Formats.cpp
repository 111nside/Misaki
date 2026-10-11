#include "../Include/GuestPS4Formats.hpp"
#include <limits>

namespace misaki { namespace {
constexpr std::uint32_t selfMagic = 0x1D3D154Fu;
constexpr std::size_t maxInspection = 8 * 1024 * 1024;
void fail(std::string *e, const char *msg) { if (e) *e = msg; }
std::uint64_t load(const std::uint8_t *b, std::size_t offset, unsigned n) {
    std::uint64_t result = 0;
    for (unsigned i = 0; i < n; ++i) result |= std::uint64_t(b[offset + i]) << (8 * i);
    return result;
}
void put(std::vector<std::uint8_t> &b, std::size_t offset, std::uint64_t value, unsigned n) {
    for (unsigned i = 0; i < n; ++i) b[offset + i] = static_cast<std::uint8_t>(value >> (8 * i));
}
bool inside(std::uint64_t start, std::uint64_t length, std::size_t size) {
    return start <= size && length <= size - start;
}
std::optional<PS4ExecutableMetadata> parseELF(const std::uint8_t *b,
        std::size_t size, std::size_t offset, PS4ExecutableMetadata out, std::string *error) {
    if (!inside(offset, 64, size) || b[offset] != 0x7f || b[offset + 1] != 'E' ||
        b[offset + 2] != 'L' || b[offset + 3] != 'F' || b[offset + 4] != 2 ||
        b[offset + 5] != 1 || b[offset + 6] != 1) {
        fail(error, "Missing ELF64 little-endian header"); return std::nullopt;
    }
    out.elfHeaderOffset = offset;
    out.elfType = static_cast<std::uint16_t>(load(b, offset + 16, 2));
    out.machine = static_cast<std::uint16_t>(load(b, offset + 18, 2));
    out.entry = load(b, offset + 24, 8);
    const auto phoff = load(b, offset + 32, 8);
    const auto headerSize = load(b, offset + 52, 2);
    const auto phent = load(b, offset + 54, 2);
    const auto phnum = load(b, offset + 56, 2);
    if (out.machine != 62 || (out.elfType != 0xFE00 && out.elfType != 0xFE10 && out.elfType != 0xFE18) ||
        headerSize != 64 || phnum > 128 || (phnum && phent != 56) ||
        phoff > std::numeric_limits<std::uint64_t>::max() - offset ||
        !inside(offset + phoff, phnum * phent, size)) {
        fail(error, "Unsupported SCE ELF architecture, type, or program-header table"); return std::nullopt;
    }
    out.programHeaders = static_cast<std::uint16_t>(phnum);
    // We deliberately do not treat PS4 PT_LOAD / encrypted SELF segments as host-loadable.
    return out;
}
} // namespace
std::optional<PS4ExecutableMetadata> inspectPS4Executable(
    const std::uint8_t *b, std::size_t size, std::string *error) {
    if (error) error->clear();
    if (!b || size < 64 || size > maxInspection) {
        fail(error, "Missing or oversized executable metadata"); return std::nullopt;
    }
    PS4ExecutableMetadata out;
    if (load(b, 0, 4) != selfMagic)
        return parseELF(b, size, 0, out, error);
    // 32-byte SELF header and 32-byte segment records (shadPS4 loader layout).
    if (b[4] != 0 || b[5] != 1 || b[6] != 1 || b[7] != 0x12 ||
        b[8] != 1 || b[9] != 1) {
        fail(error, "Unrecognized SELF header profile"); return std::nullopt;
    }
    const std::uint64_t count = load(b, 24, 2);
    const auto declaredSize = load(b, 16, 4);
    if (count == 0 || count > 128 || !inside(32, count * 32, size) ||
        (declaredSize != 0 && declaredSize > size)) {
        fail(error, "SELF segment table or declared size invalid"); return std::nullopt;
    }
    out.kind = PS4ContainerKind::self;
    out.selfSegments = static_cast<std::uint16_t>(count);
    for (std::uint64_t i = 0; i < count; ++i) {
        const auto at = 32 + i * 32;
        const auto flags = load(b, at, 8);
        const auto start = load(b, at + 8, 8);
        const auto length = load(b, at + 16, 8);
        const auto memsize = load(b, at + 24, 8);
        if (flags & ~(std::uint64_t(0xFFF) << 20 | 0x80Full) ||
            !inside(start, length, size) || memsize < length) {
            fail(error, "Unsupported SELF segment flags or out-of-range segment"); return std::nullopt;
        }
        if (flags & 2) ++out.encryptedSegments;
        if (flags & 8) ++out.compressedSegments;
    }
    return parseELF(b, size, 32 + count * 32, out, error);
}
std::vector<std::uint8_t> makePS4StructuralFixture(bool encrypted) {
    std::vector<std::uint8_t> b(0x210, 0);
    put(b, 0, selfMagic, 4);
    b[4]=0; b[5]=1; b[6]=1; b[7]=0x12; b[8]=1; b[9]=1;
    put(b, 12, 0x100, 2); put(b, 16, b.size(), 4); put(b, 24, 1, 2);
    put(b, 32, encrypted ? 2 : 0, 8); // flags (synthetic)
    put(b, 40, 0x200, 8); put(b, 48, 16, 8); put(b, 56, 16, 8);
    const std::size_t off = 64;
    b[off]=0x7f; b[off+1]='E'; b[off+2]='L'; b[off+3]='F';
    b[off+4]=2; b[off+5]=1; b[off+6]=1;
    put(b, off+16, 0xFE10, 2); put(b, off+18, 62, 2);
    put(b, off+24, 0x400000, 8); put(b, off+32, 64, 8);
    put(b, off+52, 64, 2); put(b, off+54, 56, 2); put(b, off+56, 1, 2);
    put(b, off+64, 1, 4); // PT_LOAD metadata; not executed.
    put(b, 0x200, 0xDEADBEEF, 4);
    return b;
}
} // namespace misaki
