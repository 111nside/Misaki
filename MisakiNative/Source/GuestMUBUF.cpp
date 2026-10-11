#include "../Include/GuestMUBUF.hpp"

#include <limits>
#include <sstream>
#include <utility>

namespace misaki {
namespace {
constexpr std::size_t maxWords = 128;
constexpr std::uint64_t fnvOffset = 14695981039346656037ULL;
constexpr std::uint64_t fnvPrime = 1099511628211ULL;
constexpr std::uint64_t instructionAddress = 0x120000;
constexpr std::uint64_t inputDescriptorAddress = 0x110000;
constexpr std::uint64_t outputDescriptorAddress = 0x110010;
constexpr std::uint64_t outputBase = 0x500000;

void fail(std::string *out, const char *reason) { if (out) *out = reason; }
void hash32(std::uint64_t &hash, std::uint32_t value) {
    for (unsigned shift = 0; shift < 32; shift += 8) {
        hash ^= static_cast<std::uint8_t>(value >> shift);
        hash *= fnvPrime;
    }
}
std::uint64_t stringHash(const std::string &source) {
    std::uint64_t hash = fnvOffset;
    for (auto c : source) { hash ^= static_cast<unsigned char>(c); hash *= fnvPrime; }
    return hash;
}
std::uint64_t outputHash(const std::array<std::uint32_t, 64> &data) {
    std::uint64_t hash = fnvOffset;
    for (const auto word : data) hash32(hash, word);
    return hash;
}
void put32(std::uint8_t *dst, std::uint32_t word) {
    for (unsigned i = 0; i < 4; ++i) dst[i] = static_cast<std::uint8_t>(word >> (8 * i));
}
std::optional<std::uint32_t> read32(const GuestMemory &mem, std::uint64_t address) {
    if (address > std::numeric_limits<std::uint64_t>::max() - 3) return std::nullopt;
    std::uint32_t out = 0;
    for (unsigned i = 0; i < 4; ++i) {
        const auto byte = mem.read8(address + i);
        if (!byte) return std::nullopt;
        out |= std::uint32_t(*byte) << (i * 8);
    }
    return out;
}
bool usable(const GCNBufferDescriptor &d, std::uint32_t stride) {
    return d.supportedLinearMetadata && d.strideBytes == stride &&
        d.records == 64 && (d.baseAddress & 3u) == 0 &&
        d.baseAddress <= std::numeric_limits<std::uint64_t>::max() -
            static_cast<std::uint64_t>(d.records) * stride;
}
bool executable(const MUBUFTrace &trace) {
    if (trace.instructions.size() != 2 || trace.loads != 1 || trace.stores != 1) return false;
    for (unsigned i = 0; i < 2; ++i) {
        const auto &ins = trace.instructions[i];
        if (ins.opcode != (i == 0 ? MUBUFOp::loadDword : MUBUFOp::storeDword) ||
            ins.vaddr != 4 || ins.vdata != 8 || ins.srsrc != i * 4 ||
            ins.soffset != 128 || ins.offsetBytes != 0 ||
            !ins.idxen || ins.offen || ins.glc || ins.addr64 ||
            ins.lds || ins.slc || ins.tfe) return false;
    }
    return true;
}
std::vector<std::uint8_t> bytesOf(const std::uint32_t *words, std::size_t count) {
    std::vector<std::uint8_t> bytes(count * 4);
    for (std::size_t i = 0; i < count; ++i) put32(bytes.data() + i * 4, words[i]);
    return bytes;
}
} // namespace

std::optional<MUBUFTrace> decodeMUBUFWords(const std::uint32_t *words,
                                            std::size_t wordCount,
                                            std::string *error) {
    if (error) error->clear();
    if (!words || !wordCount || wordCount > maxWords || (wordCount & 1u)) {
        fail(error, "Null, odd, or oversized MUBUF word stream");
        return std::nullopt;
    }
    MUBUFTrace out;
    out.checksum = fnvOffset;
    for (std::size_t i = 0; i < wordCount; i += 2) {
        const auto first = words[i], second = words[i + 1];
        // GCN1.0/1.1 MUBUF encoding [31:26] = 111000.
        // Require reserved bits 25, 17 and upper second-DWORD bit 21 to be zero.
        if ((first & 0xFC000000u) != 0xE0000000u ||
            (first & ((1u << 25) | (1u << 17))) != 0 ||
            (second & (1u << 21)) != 0) {
            fail(error, "Bad MUBUF tag or reserved bits");
            return std::nullopt;
        }
        const auto opcode = (first >> 18) & 0x7Fu;
        if (opcode != 12 && opcode != 28) {
            fail(error, "Unsupported MUBUF opcode (load/store DWORD only)");
            return std::nullopt;
        }
        MUBUFInstruction ins;
        ins.opcode = static_cast<MUBUFOp>(opcode);
        ins.offsetBytes = first & 0xFFFu;
        ins.offen = ((first >> 12) & 1u) != 0;
        ins.idxen = ((first >> 13) & 1u) != 0;
        ins.glc = ((first >> 14) & 1u) != 0;
        ins.addr64 = ((first >> 15) & 1u) != 0;
        ins.lds = ((first >> 16) & 1u) != 0;
        ins.vaddr = second & 0xFFu;
        ins.vdata = (second >> 8) & 0xFFu;
        ins.srsrc = ((second >> 16) & 0x1Fu) * 4u;
        ins.slc = ((second >> 22) & 1u) != 0;
        ins.tfe = ((second >> 23) & 1u) != 0;
        ins.soffset = second >> 24;
        ins.wordOffset = static_cast<std::uint32_t>(i);
        out.instructions.push_back(ins);
        if (ins.opcode == MUBUFOp::loadDword) ++out.loads;
        else ++out.stores;
        hash32(out.checksum, first);
        hash32(out.checksum, second);
    }
    return out;
}

std::optional<MUBUFTrace> decodeGuestMUBUF(const GuestMemory &guest,
                                            std::uint64_t address,
                                            std::size_t wordCount,
                                            std::string *error) {
    if (error) error->clear();
    if (!wordCount || wordCount > maxWords || (wordCount & 1u) ||
        (address & 7u) ||
        address > std::numeric_limits<std::uint64_t>::max() - (wordCount * 4 - 1)) {
        fail(error, "Invalid/misaligned guest MUBUF range");
        return std::nullopt;
    }
    std::vector<std::uint32_t> words;
    words.reserve(wordCount);
    for (std::size_t i = 0; i < wordCount; ++i) {
        const auto value = read32(guest, address + i * 4);
        if (!value) {
            fail(error, "Guest MUBUF instruction memory is unreadable");
            return std::nullopt;
        }
        words.push_back(*value);
    }
    return decodeMUBUFWords(words.data(), words.size(), error);
}

std::optional<MUBUF29Reference> executeMUBUF29(
    const MUBUFTrace &trace, GuestMemory &memory,
    std::uint64_t inputDescAddr, std::uint64_t outputDescAddr,
    std::uint32_t vaddrIndex, const std::array<std::uint32_t, 64> &initial,
    std::string *error) {
    if (error) error->clear();
    if (!executable(trace) || inputDescAddr == outputDescAddr) {
        fail(error, "Unsupported MUBUF execution pattern");
        return std::nullopt;
    }
    const auto src = decodeGuestGCNBuffer(memory, inputDescAddr);
    const auto dst = decodeGuestGCNBuffer(memory, outputDescAddr);
    if (!src || !dst || !usable(*src, 16) || !usable(*dst, 4) ||
        src->baseAddress == dst->baseAddress || vaddrIndex >= 64) {
        fail(error, "Buffer descriptor or record index invalid");
        return std::nullopt;
    }
    // Two-phase validation: input readable and destination writable BEFORE a
    // guest store; no access to host pointers or platform device memory.
    const auto value = read32(memory, src->baseAddress + vaddrIndex * 16u);
    const auto oldDestination = read32(memory, dst->baseAddress + vaddrIndex * 4u);
    if (!value || !oldDestination) {
        fail(error, "Buffer input/output memory read fault");
        return std::nullopt;
    }
    // GuestMemory::writeBytes preflights *every* destination byte before
    // copying, so a read-only or truncated output cannot be partially changed.
    // Isolated guest memory, independent of host Metal buffer copies.
    std::uint8_t bytes[4];
    put32(bytes, *value);
    if (!memory.writeBytes(dst->baseAddress + vaddrIndex * 4u, bytes, sizeof(bytes))) {
        fail(error, "Guest destination write fault");
        return std::nullopt;
    }
    MUBUF29Reference out;
    out.output = initial;
    out.output[vaddrIndex] = *value;
    out.loaded = *value;
    out.stored = *value;
    out.index = vaddrIndex;
    return out;
}

std::optional<std::string> translateMUBUF29ToMetal(
    const MUBUFTrace &trace, const GCNBufferDescriptor &src,
    const GCNBufferDescriptor &dst, std::string *error) {
    if (error) error->clear();
    if (!executable(trace) || !usable(src, 16) || !usable(dst, 4) ||
        src.baseAddress == dst.baseAddress) {
        fail(error, "Unsupported instruction or resource descriptor for Metal");
        return std::nullopt;
    }
    const auto &load = trace.instructions[0];
    const auto &store = trace.instructions[1];
    std::ostringstream out;
    out << "#include <metal_stdlib>\nusing namespace metal;\n"
           "// Decoded AMD GCN1.1 MUBUF instructions, ONE emulated lane.\n"
           "// SRSRC s[0:3] maps to guestWords; s[4:7] to outputWords.\n"
           "// Guest v4 is explicitly staged from ALU output register 0.\n"
           "kernel void misaki_mubuf29(device const uint *registers [[buffer(0)]],\n"
           "    device const uint *guestWords [[buffer(1)]],\n"
           "    device uint *outputWords [[buffer(2)]],\n"
           "    device uint *status [[buffer(3)]],\n"
           "    uint tid [[thread_position_in_grid]]) {\n"
           "    if (tid != 0u) return;\n"
           "    status[0] = 1u;\n"
           "    const uint guest_v" << load.vaddr << " = registers[0];\n"
           "    if (guest_v" << load.vaddr << " >= " << src.records << "u) return;\n"
           "    const uint guest_v" << load.vdata << " = guestWords[guest_v"
        << load.vaddr << " * " << src.strideBytes / 4 << "u + " << load.offsetBytes / 4 << "u];\n"
           "    if (guest_v" << store.vaddr << " >= " << dst.records << "u) return;\n"
           "    outputWords[guest_v" << store.vaddr << " * " << dst.strideBytes / 4 << "u + "
        << store.offsetBytes / 4 << "u] = guest_v" << store.vdata << ";\n"
           "    status[0] = 0u;\n}\n";
    if (out.str().size() + 1 > 4096) {
        fail(error, "Generated Metal source exceeds budget");
        return std::nullopt;
    }
    return out.str();
}

std::vector<std::uint32_t> makeMUBUF29Fixture() {
    // Two real-form GCN 1.1 MUBUF encodings:
    // BUFFER_LOAD_DWORD v8, v4, s[0:3], 0 idxen
    // BUFFER_STORE_DWORD v8, v4, s[4:7], 0 idxen
    const auto word0 = [](unsigned op) { return 0xE0000000u | (op << 18) | (1u << 13); };
    const auto word1 = [](unsigned slot) {
        return (128u << 24) | (slot << 16) | (8u << 8) | 4u;
    };
    return {word0(12), word1(0), word0(28), word1(1)};
}

std::optional<MUBUF29Plan> runMUBUF29Diagnostic() {
    try {
        const auto alu = runMetalIRDiagnostic();
        const auto buffer = runGCNShaderDiagnostic();
        if (!alu || !buffer || alu->expected[0] != 7) return std::nullopt;
        MUBUF29Plan result;
        result.aluRegisters = alu->expected;
        result.inputBase = buffer->buffer.baseAddress;
        result.outputBase = outputBase;
        const auto code = makeMUBUF29Fixture();
        const auto encoded = bytesOf(code.data(), code.size());
        auto srcDesc = makeGCNBufferFixture();
        auto dstDesc = srcDesc;
        dstDesc[0] = static_cast<std::uint32_t>(outputBase);
        dstDesc[1] = 4u << 16;
        const auto srcBytes = bytesOf(srcDesc.data(), srcDesc.size());
        const auto dstBytes = bytesOf(dstDesc.data(), dstDesc.size());
        GuestMemory memory;
        if (!memory.map(instructionAddress, encoded.size(), permission::read | permission::write) ||
            !memory.writeBytes(instructionAddress, encoded.data(), encoded.size()) ||
            !memory.protect(instructionAddress, encoded.size(), permission::read) ||
            !memory.map(inputDescriptorAddress, 32, permission::read | permission::write) ||
            !memory.writeBytes(inputDescriptorAddress, srcBytes.data(), srcBytes.size()) ||
            !memory.writeBytes(outputDescriptorAddress, dstBytes.data(), dstBytes.size()) ||
            !memory.protect(inputDescriptorAddress, 32, permission::read)) return std::nullopt;
        result.codeReadOnly = !memory.writeBytes(instructionAddress, encoded.data(), 1);
        result.descriptorsReadOnly = !memory.writeBytes(inputDescriptorAddress, srcBytes.data(), 1) &&
                                     !memory.writeBytes(outputDescriptorAddress, dstBytes.data(), 1);
        for (std::size_t i = 0; i < 64; ++i) {
            put32(result.inputBytes.data() + i * 16, static_cast<std::uint32_t>(i * 3 + 10));
            result.initial[i] = 0xA0000000u + static_cast<std::uint32_t>(i);
        }
        if (!memory.map(result.inputBase, result.inputBytes.size(),
                        permission::read | permission::write) ||
            !memory.writeBytes(result.inputBase, result.inputBytes.data(), result.inputBytes.size()) ||
            !memory.protect(result.inputBase, result.inputBytes.size(), permission::read)) return std::nullopt;
        result.sourceReadOnly = !memory.writeBytes(result.inputBase, result.inputBytes.data(), 1);
        const auto outInitial = bytesOf(result.initial.data(), result.initial.size());
        if (!memory.map(result.outputBase, outInitial.size(), permission::read | permission::write) ||
            !memory.writeBytes(result.outputBase, outInitial.data(), outInitial.size())) return std::nullopt;
        auto decoded = decodeGuestMUBUF(memory, instructionAddress, code.size());
        auto input = decodeGuestGCNBuffer(memory, inputDescriptorAddress);
        auto output = decodeGuestGCNBuffer(memory, outputDescriptorAddress);
        if (!decoded || !input || !output || !result.codeReadOnly ||
            !result.descriptorsReadOnly || !result.sourceReadOnly ||
            !usable(*input, 16) || !usable(*output, 4)) return std::nullopt;
        auto translated = translateMUBUF29ToMetal(*decoded, *input, *output);
        auto evaluated = executeMUBUF29(*decoded, memory, inputDescriptorAddress,
                                        outputDescriptorAddress,
                                        result.aluRegisters[0], result.initial);
        if (!translated || !evaluated) return std::nullopt;
        result.outputWritable = true;
        for (std::size_t i = 0; i < 64; ++i) {
            const auto value = read32(memory, outputBase + i * 4);
            if (!value || *value != evaluated->output[i]) result.outputWritable = false;
        }
        result.decoded = std::move(*decoded);
        result.metalSource = std::move(*translated);
        result.expected = evaluated->output;
        result.recordIndex = evaluated->index;
        result.loaded = evaluated->loaded;
        result.stored = evaluated->stored;
        result.shaderSourceHash = stringHash(result.metalSource);
        result.resultHash = outputHash(result.expected);
        if (!result.outputWritable || result.recordIndex != 7 ||
            result.loaded != 31 || result.stored != 31) return std::nullopt;
        return result;
    } catch (...) { return std::nullopt; }
}
} // namespace misaki
