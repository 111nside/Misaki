#include "../Include/GuestFusedGCN30.hpp"

#include <limits>
#include <sstream>
#include <utility>

namespace misaki {
namespace {
constexpr std::uint64_t fnvOffset = 14695981039346656037ULL;
constexpr std::uint64_t fnvPrime = 1099511628211ULL;
constexpr std::uint64_t codeBase = 0x100000;
constexpr std::uint64_t inputDesc = 0x110000;
constexpr std::uint64_t outputDesc = 0x110010;
constexpr std::uint64_t inputBase = 0x400000;
constexpr std::uint64_t outputBase = 0x500000;
constexpr std::size_t maxCodeWords = 64;
constexpr std::uint32_t endProgramWord = 0xBF810000u;

void fail(std::string *error, const char *reason) { if (error) *error = reason; }
void hash32(std::uint64_t &hash, std::uint32_t word) {
    for (unsigned shift = 0; shift < 32; shift += 8) {
        hash ^= static_cast<std::uint8_t>(word >> shift);
        hash *= fnvPrime;
    }
}
std::uint64_t hashText(const std::string &str) {
    std::uint64_t hash = fnvOffset;
    for (unsigned char c : str) { hash ^= c; hash *= fnvPrime; }
    return hash;
}
std::uint64_t hashArray(const std::array<std::uint32_t, 64> &array) {
    std::uint64_t hash = fnvOffset;
    for (auto x : array) hash32(hash, x);
    return hash;
}
void put32(std::uint8_t *dst, std::uint32_t word) {
    for (unsigned i = 0; i < 4; ++i) dst[i] = static_cast<std::uint8_t>(word >> (8 * i));
}
std::vector<std::uint8_t> asBytes(const std::uint32_t *words, std::size_t count) {
    std::vector<std::uint8_t> out(count * 4);
    for (std::size_t i = 0; i < count; ++i) put32(out.data() + 4 * i, words[i]);
    return out;
}
std::optional<std::uint32_t> read32(const GuestMemory &memory, std::uint64_t address) {
    if (address > std::numeric_limits<std::uint64_t>::max() - 3) return std::nullopt;
    std::uint32_t result = 0;
    for (unsigned i = 0; i < 4; ++i) {
        auto byte = memory.read8(address + i);
        if (!byte) return std::nullopt;
        result |= static_cast<std::uint32_t>(*byte) << (i * 8);
    }
    return result;
}
bool resourceUsable(const GCNBufferDescriptor &descriptor, std::uint32_t expectedStride) {
    return descriptor.supportedLinearMetadata && descriptor.strideBytes == expectedStride &&
        descriptor.records == 64 && (descriptor.baseAddress & 3u) == 0 &&
        descriptor.baseAddress <= std::numeric_limits<std::uint64_t>::max() -
            static_cast<std::uint64_t>(descriptor.records) * descriptor.strideBytes;
}
bool validMemory(const MUBUFInstruction &instruction) {
    return instruction.idxen && !instruction.offen && !instruction.addr64 &&
        !instruction.glc && !instruction.lds && !instruction.slc &&
        !instruction.tfe && instruction.soffset == 128 &&
        instruction.offsetBytes == 0 && instruction.vaddr == 4 &&
        ((instruction.opcode == MUBUFOp::loadDword &&
          instruction.vdata == 8 && instruction.srsrc == 0) ||
         (instruction.opcode == MUBUFOp::storeDword &&
          instruction.vdata == 9 && instruction.srsrc == 4));
}
bool supportedALU(const ShaderIRInstruction &ins) {
    switch (ins.op) {
    case ShaderIROp::scalarMovImmediate:
    case ShaderIROp::scalarMov:
    case ShaderIROp::scalarAdd:
        return ins.destination < 102;
    case ShaderIROp::vectorMov:
        return ins.destination < 256;
    default: return false;
    }
}
std::optional<std::uint32_t> sourceValue(const ShaderIRSource &source,
    const ShaderIRInputs &state) {
    switch (source.kind) {
    case ShaderIRSourceKind::scalarRegister:
        if (source.value < state.scalar.size()) return state.scalar[source.value];
        break;
    case ShaderIRSourceKind::vectorRegister:
        if (source.value < state.vector.size()) return state.vector[source.value];
        break;
    case ShaderIRSourceKind::immediateBits: return source.value;
    case ShaderIRSourceKind::none: break;
    }
    return std::nullopt;
}
std::optional<std::string> mslSource(const ShaderIRSource &source) {
    switch (source.kind) {
    case ShaderIRSourceKind::scalarRegister:
        if (source.value < 102) return "s[" + std::to_string(source.value) + "]";
        break;
    case ShaderIRSourceKind::vectorRegister:
        if (source.value < 256) return "v[" + std::to_string(source.value) + "]";
        break;
    case ShaderIRSourceKind::immediateBits:
        return std::to_string(source.value) + "u";
    case ShaderIRSourceKind::none: break;
    }
    return std::nullopt;
}
bool validateSequence(const FusedGCN30Program &program) {
    if (program.steps.size() < 5 || program.steps.size() > maxCodeWords ||
        program.steps.back().kind != FusedGCN30Kind::end ||
        program.aluOperations + program.bufferLoads + program.bufferStores + 1 != program.steps.size() ||
        program.bufferLoads != 1 || program.bufferStores != 1 || program.checksum == 0) return false;
    std::uint32_t expectedWord = 0;
    bool loaded = false, stored = false;
    for (std::size_t i = 0; i < program.steps.size(); ++i) {
        const auto &step = program.steps[i];
        if (step.wordOffset != expectedWord) return false;
        if (step.kind == FusedGCN30Kind::alu) {
            if (stored || !supportedALU(step.alu) ||
                step.alu.wordOffset != step.wordOffset) return false;
            expectedWord += 1;
        } else if (step.kind == FusedGCN30Kind::memory) {
            if (!validMemory(step.memory) || step.memory.wordOffset != step.wordOffset) return false;
            if (step.memory.opcode == MUBUFOp::loadDword) {
                if (loaded || stored) return false;
                loaded = true;
            } else {
                if (!loaded || stored) return false;
                stored = true;
            }
            expectedWord += 2;
        } else if (step.kind == FusedGCN30Kind::end) {
            if (!loaded || !stored || i != program.steps.size() - 1) return false;
            ++expectedWord;
        } else return false;
    }
    return expectedWord == program.codeWords;
}
} // namespace

std::optional<FusedGCN30Program> decodeFusedGCN30(
    const std::uint32_t *words, std::size_t count, std::uint64_t address,
    std::string *error) {
    if (error) error->clear();
    if (!words || count < 5 || count > maxCodeWords || (address & 7u) != 0 ||
        address > std::numeric_limits<std::uint64_t>::max() - (count * 4 - 1)) {
        fail(error, "Invalid GCN shader range"); return std::nullopt;
    }
    FusedGCN30Program program;
    program.codeWords = static_cast<std::uint32_t>(count);
    program.checksum = fnvOffset;
    std::size_t at = 0;
    while (at < count) {
        if (program.steps.size() >= maxCodeWords) {
            fail(error, "Instruction count exceeds budget"); return std::nullopt;
        }
        FusedGCN30Step step;
        step.wordOffset = static_cast<std::uint32_t>(at);
        const std::uint32_t word = words[at];
        if ((word & 0xFC000000u) == 0xE0000000u) {
            if (at + 1 >= count) {
                fail(error, "Truncated GCN memory instruction"); return std::nullopt;
            }
            const auto decoded = decodeMUBUFWords(words + at, 2, error);
            if (!decoded || decoded->instructions.size() != 1 ||
                !validMemory(decoded->instructions[0])) {
                fail(error, "Unsupported MUBUF instruction mode"); return std::nullopt;
            }
            step.kind = FusedGCN30Kind::memory;
            step.memory = decoded->instructions[0];
            step.memory.wordOffset = static_cast<std::uint32_t>(at);
            if (step.memory.opcode == MUBUFOp::loadDword) ++program.bufferLoads;
            else ++program.bufferStores;
            hash32(program.checksum, word);
            hash32(program.checksum, words[at + 1]);
            at += 2;
        } else {
            // The earlier ISA decoder requires an exact stream ending in
            // S_ENDPGM. Reuse its validation for each single-DWORD ALU opcode
            // by supplying a synthetic terminator *to the decoder only*.
            // The source words still come exclusively from caller/guest memory.
            const std::uint32_t fragment[] = {word, endProgramWord};
            const auto decoded = decodeGCNShaderWords(
                fragment, word == endProgramWord ? 1u : 2u,
                address + at * 4, error);
            if (!decoded) return std::nullopt;
            const auto lowered = lowerGCNToShaderIR(*decoded, error);
            if (!lowered || lowered->instructions.empty()) return std::nullopt;
            const auto &ir = lowered->instructions.front();
            if (ir.op == ShaderIROp::end) {
                if (at != count - 1) { fail(error, "Early shader termination"); return std::nullopt; }
                step.kind = FusedGCN30Kind::end;
            } else {
                if (ir.op == ShaderIROp::nop || !supportedALU(ir)) {
                    fail(error, "Unsupported fused GCN scalar/vector ALU instruction");
                    return std::nullopt;
                }
                step.kind = FusedGCN30Kind::alu;
                step.alu = ir;
                step.alu.wordOffset = static_cast<std::uint32_t>(at);
                ++program.aluOperations;
            }
            hash32(program.checksum, word);
            ++at;
        }
        program.steps.push_back(step);
    }
    if (!validateSequence(program)) {
        fail(error, "Fused GCN shader has invalid instruction order or operands");
        return std::nullopt;
    }
    return program;
}

std::optional<FusedGCN30Program> decodeGuestFusedGCN30(
    const GuestMemory &memory, std::uint64_t address, std::size_t count,
    std::string *error) {
    if (error) error->clear();
    if (!count || count > maxCodeWords || (address & 7u) != 0 ||
        address > std::numeric_limits<std::uint64_t>::max() - (count * 4 - 1)) {
        fail(error, "Invalid guest GCN address/size"); return std::nullopt;
    }
    std::vector<std::uint32_t> words;
    words.reserve(count);
    for (std::size_t i = 0; i < count; ++i) {
        const auto value = read32(memory, address + i * 4);
        if (!value) { fail(error, "Unmapped/unreadable guest shader"); return std::nullopt; }
        words.push_back(*value);
    }
    return decodeFusedGCN30(words.data(), words.size(), address, error);
}

std::optional<FusedGCN30Reference> executeFusedGCN30(
    const FusedGCN30Program &program, GuestMemory &memory,
    std::uint64_t inputDescriptor, std::uint64_t outputDescriptor,
    const std::array<std::uint32_t, 64> &initial,
    std::string *error) {
    if (error) error->clear();
    if (!validateSequence(program) || inputDescriptor == outputDescriptor) {
        fail(error, "Unsupported fused execution plan"); return std::nullopt;
    }
    const auto input = decodeGuestGCNBuffer(memory, inputDescriptor);
    const auto output = decodeGuestGCNBuffer(memory, outputDescriptor);
    if (!input || !output || !resourceUsable(*input, 16) ||
        !resourceUsable(*output, 4) || input->baseAddress == output->baseAddress) {
        fail(error, "Invalid buffer descriptors"); return std::nullopt;
    }
    ShaderIRInputs state;
    FusedGCN30Reference ref;
    ref.output = initial;
    bool loaded = false, stored = false;
    for (const auto &step : program.steps) {
        if (step.kind == FusedGCN30Kind::alu) {
            const auto a = sourceValue(step.alu.a, state);
            if (!a) { fail(error, "Bad ALU source"); return std::nullopt; }
            std::uint32_t result = *a;
            if (step.alu.op == ShaderIROp::scalarAdd) {
                const auto b = sourceValue(step.alu.b, state);
                if (!b) { fail(error, "Bad second ALU source"); return std::nullopt; }
                result += *b; // intentional uint32 wraparound
            }
            if (step.alu.op == ShaderIROp::vectorMov)
                state.vector[step.alu.destination] = result;
            else state.scalar[step.alu.destination] = result;
        } else if (step.kind == FusedGCN30Kind::memory) {
            const auto &ins = step.memory;
            const auto index = state.vector[ins.vaddr];
            if (index >= 64) { fail(error, "Buffer record index out of range"); return std::nullopt; }
            ref.index = index;
            if (ins.opcode == MUBUFOp::loadDword) {
                const auto value = read32(memory, input->baseAddress + index * 16u);
                if (!value) { fail(error, "Source buffer unreadable"); return std::nullopt; }
                state.vector[ins.vdata] = *value;
                ref.loaded = *value;
                loaded = true;
            } else {
                if (!loaded || stored) { fail(error, "MUBUF store precedes load"); return std::nullopt; }
                const auto destination = output->baseAddress + index * 4u;
                // Require the old destination to be readable. The later
                // writeBytes call preflights all bytes before writing.
                if (!read32(memory, destination)) {
                    fail(error, "Destination buffer unreadable"); return std::nullopt;
                }
                const auto value = state.vector[ins.vdata];
                std::uint8_t raw[4];
                put32(raw, value);
                if (!memory.writeBytes(destination, raw, sizeof(raw))) {
                    fail(error, "Destination buffer not writable"); return std::nullopt;
                }
                ref.stored = value;
                ref.output[index] = value;
                stored = true;
            }
        } else if (step.kind != FusedGCN30Kind::end) return std::nullopt;
        ++ref.executed;
    }
    if (!loaded || !stored) return std::nullopt;
    ref.registers = {{state.scalar[1], state.scalar[2], state.scalar[3],
        state.scalar[10], state.scalar[12], state.vector[4], state.vector[8], state.vector[9]}};
    return ref;
}

std::optional<std::string> translateFusedGCN30ToMetal(
    const FusedGCN30Program &program, const GCNBufferDescriptor &input,
    const GCNBufferDescriptor &output, std::string *error) {
    if (error) error->clear();
    if (!validateSequence(program) || !resourceUsable(input, 16) ||
        !resourceUsable(output, 4) || input.baseAddress == output.baseAddress) {
        fail(error, "Unsupported fused GCN program or resource metadata");
        return std::nullopt;
    }
    std::ostringstream out;
    out << "#include <metal_stdlib>\nusing namespace metal;\n"
           "// Real-format GCN ALU and MUBUF decode; restricted ONE-LANE test.\n"
           "kernel void misaki_fused30(device const uint *inputWords [[buffer(0)]],\n"
           "    device uint *outputWords [[buffer(1)]],\n"
           "    device uint *resultRegisters [[buffer(2)]],\n"
           "    device uint *status [[buffer(3)]],\n"
           "    uint tid [[thread_position_in_grid]]) {\n"
           "    if (tid != 0u) return;\n"
           "    status[0] = 1u;\n"
           "    uint s[102] = {};\n    uint v[256] = {};\n";
    for (const auto &step : program.steps) {
        if (step.kind == FusedGCN30Kind::alu) {
            const auto a = mslSource(step.alu.a);
            if (!a) { fail(error, "Unsupported Metal ALU source"); return std::nullopt; }
            auto expr = *a;
            if (step.alu.op == ShaderIROp::scalarAdd) {
                const auto b = mslSource(step.alu.b);
                if (!b) { fail(error, "Unsupported Metal source B"); return std::nullopt; }
                expr = "(" + *a + " + " + *b + ")";
            }
            const bool vector = step.alu.op == ShaderIROp::vectorMov;
            out << "    " << (vector ? "v[" : "s[") << step.alu.destination
                << "] = " << expr << ";\n";
        } else if (step.kind == FusedGCN30Kind::memory) {
            const auto &ins = step.memory;
            out << "    if (v[" << ins.vaddr << "] >= "
                << (ins.opcode == MUBUFOp::loadDword ? input.records : output.records)
                << "u) return;\n";
            if (ins.opcode == MUBUFOp::loadDword) {
                out << "    v[" << ins.vdata << "] = inputWords[v[" << ins.vaddr
                    << "] * " << input.strideBytes / 4 << "u];\n";
            } else {
                out << "    outputWords[v[" << ins.vaddr << "] * "
                    << output.strideBytes / 4 << "u] = v[" << ins.vdata << "];\n";
            }
        }
    }
    out << "    resultRegisters[0] = s[1];\n"
           "    resultRegisters[1] = s[2];\n"
           "    resultRegisters[2] = s[3];\n"
           "    resultRegisters[3] = s[10];\n"
           "    resultRegisters[4] = s[12];\n"
           "    resultRegisters[5] = v[4];\n"
           "    resultRegisters[6] = v[8];\n"
           "    resultRegisters[7] = v[9];\n"
           "    status[0] = 0u;\n}\n";
    if (out.str().size() + 1 > 8192) { fail(error, "Metal output exceeds budget"); return std::nullopt; }
    return out.str();
}

std::vector<std::uint32_t> makeFusedGCN30Fixture() {
    const auto mem = makeMUBUF29Fixture();
    // Original GCN1.1 instruction encodings, in real execution order.
    // s_movk s1,7; s_mov s2,s1; s_add s3,s1,s2; s_movk s11,4;
    // s_add s12,s3,s11; v_mov v4,s1; BUFFER_LOAD_DWORD v8,v4,s[0:3];
    // v_mov v9,v8; s_add s10,s12,s2; BUFFER_STORE_DWORD v9,v4,s[4:7];
    // s_endpgm.
    return {
        0xB0000000u | (1u << 16) | 7u,
        0xBE800000u | (2u << 16) | 1u,
        0x80000000u | (3u << 16) | (2u << 8) | 1u,
        0xB0000000u | (11u << 16) | 4u,
        0x80000000u | (12u << 16) | (11u << 8) | 3u,
        0x7E000000u | (4u << 17) | (1u << 9) | 1u,
        mem[0], mem[1],
        0x7E000000u | (9u << 17) | (1u << 9) | (256u + 8u),
        0x80000000u | (10u << 16) | (2u << 8) | 12u,
        mem[2], (mem[3] & ~(0xFFu << 8)) | (9u << 8),
        endProgramWord
    };
}

std::optional<FusedGCN30Plan> runFusedGCN30Diagnostic() {
    try {
        const auto words = makeFusedGCN30Fixture();
        const auto codeBytes = asBytes(words.data(), words.size());
        GuestMemory memory;
        if (!memory.map(codeBase, codeBytes.size(), permission::read | permission::write) ||
            !memory.writeBytes(codeBase, codeBytes.data(), codeBytes.size()) ||
            !memory.protect(codeBase, codeBytes.size(), permission::read)) return std::nullopt;
        FusedGCN30Plan result;
        result.codeReadOnly = !memory.writeBytes(codeBase, codeBytes.data(), 1);
        auto src = makeGCNBufferFixture();
        auto dst = src;
        dst[0] = static_cast<std::uint32_t>(outputBase);
        dst[1] = 4u << 16;
        const auto srcBytes = asBytes(src.data(), src.size());
        const auto dstBytes = asBytes(dst.data(), dst.size());
        if (!memory.map(inputDesc, 32, permission::read | permission::write) ||
            !memory.writeBytes(inputDesc, srcBytes.data(), srcBytes.size()) ||
            !memory.writeBytes(outputDesc, dstBytes.data(), dstBytes.size()) ||
            !memory.protect(inputDesc, 32, permission::read)) return std::nullopt;
        result.descriptorsReadOnly = !memory.writeBytes(inputDesc, srcBytes.data(), 1) &&
                                     !memory.writeBytes(outputDesc, dstBytes.data(), 1);
        result.inputBase = inputBase;
        result.outputBase = outputBase;
        for (std::size_t i = 0; i < 64; ++i) {
            put32(result.inputBytes.data() + i * 16, static_cast<std::uint32_t>(i * 3 + 10));
            result.initial[i] = 0xB0000000u + static_cast<std::uint32_t>(i);
        }
        if (!memory.map(inputBase, result.inputBytes.size(), permission::read | permission::write) ||
            !memory.writeBytes(inputBase, result.inputBytes.data(), result.inputBytes.size()) ||
            !memory.protect(inputBase, result.inputBytes.size(), permission::read)) return std::nullopt;
        result.inputReadOnly = !memory.writeBytes(inputBase, result.inputBytes.data(), 1);
        const auto initialBytes = asBytes(result.initial.data(), result.initial.size());
        if (!memory.map(outputBase, initialBytes.size(), permission::read | permission::write) ||
            !memory.writeBytes(outputBase, initialBytes.data(), initialBytes.size())) return std::nullopt;
        const auto program = decodeGuestFusedGCN30(memory, codeBase, words.size());
        const auto input = decodeGuestGCNBuffer(memory, inputDesc);
        const auto output = decodeGuestGCNBuffer(memory, outputDesc);
        if (!program || !input || !output || !result.codeReadOnly ||
            !result.descriptorsReadOnly || !result.inputReadOnly) return std::nullopt;
        const auto metal = translateFusedGCN30ToMetal(*program, *input, *output);
        const auto evaluation = executeFusedGCN30(*program, memory, inputDesc, outputDesc, result.initial);
        if (!metal || !evaluation) return std::nullopt;
        result.outputWritable = true;
        for (std::size_t i = 0; i < 64; ++i) {
            const auto value = read32(memory, outputBase + i * 4);
            if (!value || *value != evaluation->output[i]) result.outputWritable = false;
        }
        result.decoded = *program;
        result.metalSource = *metal;
        result.expected = *evaluation;
        result.metalSourceHash = hashText(*metal);
        result.expectedHash = hashArray(evaluation->output);
        if (!result.outputWritable || evaluation->executed != 11 ||
            evaluation->index != 7 || evaluation->loaded != 31 ||
            evaluation->stored != 31 || evaluation->registers !=
                std::array<std::uint32_t, 8>{{7,7,14,25,18,7,31,31}}) return std::nullopt;
        return result;
    } catch (...) { return std::nullopt; }
}
} // namespace misaki
