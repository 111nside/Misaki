#include "../Include/GuestGCNISA.hpp"

#include <limits>
#include <utility>

namespace misaki {
namespace {
constexpr std::size_t maxWords = 1024;
constexpr std::size_t maxInstructions = 256;
constexpr std::uint64_t fnvOffset = 14695981039346656037ULL;
constexpr std::uint64_t fnvPrime = 1099511628211ULL;
void fail(std::string *err, const char *why) { if (err) *err = why; }
void hashWord(std::uint64_t &hash, std::uint32_t word) {
    for (unsigned i = 0; i < 32; i += 8) {
        hash ^= static_cast<std::uint8_t>(word >> i);
        hash *= fnvPrime;
    }
}
std::optional<std::uint32_t> guestWord(const GuestMemory &memory, std::uint64_t address) {
    if (address > std::numeric_limits<std::uint64_t>::max() - 3) return std::nullopt;
    std::uint32_t value = 0;
    for (unsigned i = 0; i < 4; ++i) {
        const auto b = memory.read8(address + i);
        if (!b) return std::nullopt;
        value |= std::uint32_t(*b) << (8 * i);
    }
    return value;
}
bool scalarSource(std::uint32_t src) {
    // Narrow scalar subset: SGPRs, integer inline constants, literal marker.
    return src <= 101 || (src >= 128 && src <= 208) || src == 255;
}
bool vectorSource(std::uint32_t src) {
    return scalarSource(src) || (src >= 256 && src <= 511);
}
std::vector<std::uint8_t> littleBytes(const std::uint32_t *words, std::size_t count) {
    std::vector<std::uint8_t> bytes;
    bytes.reserve(count * 4);
    for (std::size_t i = 0; i < count; ++i)
        for (unsigned shift = 0; shift < 32; shift += 8)
            bytes.push_back(static_cast<std::uint8_t>(words[i] >> shift));
    return bytes;
}
} // namespace

std::optional<GCNShaderTrace> decodeGCNShaderWords(
    const std::uint32_t *words, std::size_t count, std::uint64_t address,
    std::string *error) {
    if (error) error->clear();
    if (!words || count == 0 || count > maxWords || (address & 3u) ||
        address > std::numeric_limits<std::uint64_t>::max() - (count * 4 - 1)) {
        fail(error, "Null, misaligned, overflowing or oversized shader stream");
        return std::nullopt;
    }
    GCNShaderTrace trace;
    trace.checksum = fnvOffset;
    std::size_t offset = 0;
    while (offset < count) {
        if (trace.instructions.size() >= maxInstructions) {
            fail(error, "Instruction budget exceeded");
            return std::nullopt;
        }
        const std::uint32_t word = words[offset];
        GCNInstruction ins;
        ins.guestAddress = address + offset * 4;
        ins.wordOffset = static_cast<std::uint32_t>(offset);
        ins.wordCount = 1;
        bool valid = false;
        bool scalar = true;
        bool final = false;
        // Decode most-specific 32-bit encodings before the SOP2/VOP2 families.
        if ((word & 0xFF800000u) == 0xBF800000u) { // SOPP
            ins.encoding = GCNEncoding::sopp;
            ins.opcode = (word >> 16) & 0x7Fu;
            ins.src0 = word & 0xFFFFu; // signed SIMM16 bit pattern
            if (ins.opcode == 0) { ins.mnemonic = "s_nop"; valid = (ins.src0 <= 15); }
            if (ins.opcode == 1) { ins.mnemonic = "s_endpgm"; valid = (ins.src0 == 0); final = valid; }
        } else if ((word & 0xFF800000u) == 0xBE800000u) { // SOP1
            ins.encoding = GCNEncoding::sop1;
            ins.opcode = (word >> 8) & 0xFFu;
            ins.dst = (word >> 16) & 0x7Fu;
            ins.src0 = word & 0xFFu;
            if (ins.opcode == 0) ins.mnemonic = "s_mov_b32";
            if (ins.opcode == 4) ins.mnemonic = "s_not_b32";
            valid = !ins.mnemonic.empty() && ins.dst <= 101 && scalarSource(ins.src0);
        } else if ((word & 0xF0000000u) == 0xB0000000u) { // SOPK
            ins.encoding = GCNEncoding::sopk;
            ins.opcode = (word >> 23) & 0x1Fu;
            ins.dst = (word >> 16) & 0x7Fu;
            ins.src0 = word & 0xFFFFu;
            if (ins.opcode == 0) ins.mnemonic = "s_movk_i32";
            valid = ins.opcode == 0 && ins.dst <= 101;
        } else if ((word & 0xC0000000u) == 0x80000000u) { // SOP2
            ins.encoding = GCNEncoding::sop2;
            ins.opcode = (word >> 23) & 0x7Fu;
            ins.dst = (word >> 16) & 0x7Fu;
            ins.src0 = word & 0xFFu;
            ins.src1 = (word >> 8) & 0xFFu;
            if (ins.opcode == 0) ins.mnemonic = "s_add_u32";
            if (ins.opcode == 1) ins.mnemonic = "s_sub_u32";
            if (ins.opcode == 12) ins.mnemonic = "s_and_b32";
            valid = !ins.mnemonic.empty() && ins.dst <= 101 &&
                    scalarSource(ins.src0) && scalarSource(ins.src1) &&
                    !(ins.src0 == 255 && ins.src1 == 255);
        } else if ((word & 0xFE000000u) == 0x7E000000u) { // VOP1
            ins.encoding = GCNEncoding::vop1;
            ins.opcode = (word >> 9) & 0xFFu;
            ins.dst = (word >> 17) & 0xFFu;
            ins.src0 = word & 0x1FFu;
            if (ins.opcode == 1) ins.mnemonic = "v_mov_b32";
            valid = ins.opcode == 1 && vectorSource(ins.src0);
            scalar = false;
        } else if ((word & 0x80000000u) == 0u) { // VOP2
            ins.encoding = GCNEncoding::vop2;
            ins.opcode = (word >> 25) & 0x3Fu;
            ins.dst = (word >> 17) & 0xFFu;
            ins.src0 = word & 0x1FFu;
            ins.src1 = (word >> 9) & 0xFFu;
            if (ins.opcode == 1) ins.mnemonic = "v_add_f32";
            if (ins.opcode == 5) ins.mnemonic = "v_mul_f32";
            valid = !ins.mnemonic.empty() && vectorSource(ins.src0);
            scalar = false;
        }
        if (!valid) {
            fail(error, "Unsupported GCN encoding/opcode/operand (not executed)");
            return std::nullopt;
        }
        // The supported subset uses at most one trailing literal DWORD.
        // Do not mistake literal data for another opcode or access past bounds.
        if (ins.encoding == GCNEncoding::sop1 || ins.encoding == GCNEncoding::sop2 ||
            ins.encoding == GCNEncoding::vop1 || ins.encoding == GCNEncoding::vop2) {
            const bool literal = ins.src0 == 255 ||
                (ins.encoding == GCNEncoding::sop2 && ins.src1 == 255);
            if (literal) {
                if (offset + 1 >= count) {
                    fail(error, "Truncated literal constant");
                    return std::nullopt;
                }
                ins.hasLiteral = true;
                ins.literal = words[offset + 1];
                ins.wordCount = 2;
                ++trace.literalWords;
            }
        }
        for (std::uint32_t j = 0; j < ins.wordCount; ++j)
            hashWord(trace.checksum, words[offset + j]);
        trace.instructions.push_back(std::move(ins));
        if (scalar) ++trace.scalarInstructions;
        else ++trace.vectorInstructions;
        offset += trace.instructions.back().wordCount;
        if (final) {
            if (offset != count) {
                fail(error, "Trailing data after S_ENDPGM in exact-size stream");
                return std::nullopt;
            }
            trace.ended = true;
            break;
        }
    }
    if (!trace.ended) {
        fail(error, "S_ENDPGM required to terminate bounded shader trace");
        return std::nullopt;
    }
    trace.totalWords = static_cast<std::uint32_t>(count);
    return trace;
}

std::optional<GCNShaderTrace> decodeGuestGCNShader(
    const GuestMemory &memory, std::uint64_t address, std::size_t count,
    std::string *error) {
    if (error) error->clear();
    if (count == 0 || count > maxWords || (address & 3u) ||
        address > std::numeric_limits<std::uint64_t>::max() - (count * 4 - 1)) {
        fail(error, "Invalid guest shader span"); return std::nullopt;
    }
    std::vector<std::uint32_t> words;
    words.reserve(count);
    for (std::size_t i = 0; i < count; ++i) {
        const auto word = guestWord(memory, address + i * 4);
        if (!word) { fail(error, "Unreadable shader guest memory"); return std::nullopt; }
        words.push_back(*word);
    }
    return decodeGCNShaderWords(words.data(), words.size(), address, error);
}

GCNBufferDescriptor decodeGCNBufferWords(const std::array<std::uint32_t, 4> &w) {
    GCNBufferDescriptor d;
    d.baseAddress = (std::uint64_t(w[1] & 0xFFFFu) << 32) | w[0];
    d.strideBytes = (w[1] >> 16) & 0x3FFFu;
    d.swizzleEnabled = (w[1] & (1u << 31)) != 0;
    d.records = w[2];
    for (unsigned i = 0; i < 4; ++i)
        d.channelSelectors[i] = (w[3] >> (3 * i)) & 7u;
    d.numericFormat = (w[3] >> 12) & 7u;
    d.dataFormat = (w[3] >> 15) & 15u;
    d.addThreadID = (w[3] & (1u << 23)) != 0;
    d.resourceType = w[3] >> 30;
    bool selectorsValid = true;
    for (auto sel : d.channelSelectors)
        selectorsValid &= sel == 0 || sel == 1 || (sel >= 4 && sel <= 7);
    d.supportedLinearMetadata = d.baseAddress != 0 && d.records != 0 &&
        d.strideBytes != 0 && d.resourceType == 0 && !d.swizzleEnabled &&
        !d.addThreadID && d.dataFormat != 0 && d.numericFormat <= 6 &&
        selectorsValid && (w[3] & (1u << 24)) == 0;
    return d;
}

std::optional<GCNBufferDescriptor> decodeGuestGCNBuffer(
    const GuestMemory &memory, std::uint64_t address) {
    if ((address & 15u) || address > std::numeric_limits<std::uint64_t>::max() - 15)
        return std::nullopt;
    std::array<std::uint32_t, 4> words{};
    for (unsigned i = 0; i < words.size(); ++i) {
        const auto word = guestWord(memory, address + i * 4);
        if (!word) return std::nullopt;
        words[i] = *word;
    }
    return decodeGCNBufferWords(words);
}

std::vector<std::uint32_t> makeGCNShaderFixture() {
    return {
        0xB0000000u | (1u << 16) | 7u,                      // s_movk_i32 s1, 7
        0xBE800000u | (2u << 16) | 1u,                      // s_mov_b32 s2, s1
        0x80000000u | (3u << 16) | (2u << 8) | 1u,         // s_add_u32 s3, s1, s2
        0x7E000000u | (1u << 9) | 257u,                    // v_mov_b32 v0, v1
        (1u << 25) | (1u << 17) | 256u,                    // v_add_f32 v1, v0, v0
        (5u << 25) | (2u << 17) | (1u << 9) | 255u,        // v_mul_f32 v2, literal, v1
        0x3F800000u,                                       // literal 1.0f
        0xBF800000u,                                       // s_nop 0
        0xBF810000u,                                       // s_endpgm
    };
}

std::array<std::uint32_t, 4> makeGCNBufferFixture() {
    return {{0x00400000u, 16u << 16, 64u,
             (4u | (5u << 3) | (6u << 6) | (7u << 9) | (14u << 15))}};
}

std::optional<GCNShaderDiagnostic> runGCNShaderDiagnostic() {
    try {
        const auto fixture = makePM4IndirectFixture();
        GuestMemory memory;
        if (!installPM4IndirectFixture(memory, fixture)) return std::nullopt;
        const auto state = analyzePM4DrawStates(memory, fixture.root, fixture.rootWords.size());
        if (!state || state->draws.size() != 1 || state->readyDraws != 1)
            return std::nullopt;
        const auto shaderAddress = state->draws[0].pixelShaderAddress;
        const auto program = makeGCNShaderFixture();
        const auto bytes = littleBytes(program.data(), program.size());
        constexpr std::uint64_t descriptorAddress = 0x110000;
        const auto resource = makeGCNBufferFixture();
        const auto resourceBytes = littleBytes(resource.data(), resource.size());
        if (!memory.map(shaderAddress, bytes.size(), permission::read | permission::write) ||
            !memory.writeBytes(shaderAddress, bytes.data(), bytes.size()) ||
            !memory.protect(shaderAddress, bytes.size(), permission::read) ||
            !memory.map(descriptorAddress, resourceBytes.size(), permission::read | permission::write) ||
            !memory.writeBytes(descriptorAddress, resourceBytes.data(), resourceBytes.size()) ||
            !memory.protect(descriptorAddress, resourceBytes.size(), permission::read))
            return std::nullopt;
        GCNShaderDiagnostic result;
        result.pm4Packets = state->packets;
        result.pm4Draws = static_cast<std::uint32_t>(state->draws.size());
        result.pixelShaderAddress = shaderAddress;
        result.shaderReadOnly = !memory.writeBytes(shaderAddress, bytes.data(), 1);
        result.descriptorReadOnly = !memory.writeBytes(descriptorAddress, resourceBytes.data(), 1);
        const auto shader = decodeGuestGCNShader(memory, shaderAddress, program.size());
        const auto descriptor = decodeGuestGCNBuffer(memory, descriptorAddress);
        if (!shader || !descriptor || !descriptor->supportedLinearMetadata ||
            !result.shaderReadOnly || !result.descriptorReadOnly) return std::nullopt;
        result.shader = *shader;
        result.buffer = *descriptor;
        return result;
    } catch (...) { return std::nullopt; }
}

} // namespace misaki
