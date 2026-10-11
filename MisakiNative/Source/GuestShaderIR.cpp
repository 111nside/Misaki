#include "../Include/GuestShaderIR.hpp"

#include <cmath>
#include <cstring>
#include <limits>
#include <utility>

namespace misaki {
namespace {
constexpr std::uint64_t fnvOffset = 14695981039346656037ULL;
constexpr std::uint64_t fnvPrime = 1099511628211ULL;
constexpr std::size_t maxInstructions = 256;
void fail(std::string *e, const char *message) { if (e) *e = message; }
void hash32(std::uint64_t &h, std::uint32_t w) {
    for (unsigned shift = 0; shift < 32; shift += 8) {
        h ^= static_cast<std::uint8_t>(w >> shift);
        h *= fnvPrime;
    }
}
void hash64(std::uint64_t &h, std::uint64_t w) {
    hash32(h, static_cast<std::uint32_t>(w));
    hash32(h, static_cast<std::uint32_t>(w >> 32));
}
std::uint32_t signExtend16(std::uint32_t bits) {
    return (bits & 0x8000u) ? (0xFFFF0000u | (bits & 0xFFFFu)) : (bits & 0xFFFFu);
}
std::optional<ShaderIRSource> scalarSource(std::uint32_t selector,
                                          const GCNInstruction &ins) {
    if (selector <= 101) return ShaderIRSource{ShaderIRSourceKind::scalarRegister, selector};
    if (selector >= 128 && selector <= 192)
        return ShaderIRSource{ShaderIRSourceKind::immediateBits, selector - 128};
    if (selector >= 193 && selector <= 208)
        return ShaderIRSource{ShaderIRSourceKind::immediateBits,
                              std::uint32_t(0) - (selector - 192)};
    if (selector == 255 && ins.hasLiteral)
        return ShaderIRSource{ShaderIRSourceKind::immediateBits, ins.literal};
    return std::nullopt;
}
std::optional<ShaderIRSource> vectorSource(std::uint32_t selector,
                                          const GCNInstruction &ins) {
    if (selector >= 256 && selector <= 511)
        return ShaderIRSource{ShaderIRSourceKind::vectorRegister, selector - 256};
    return scalarSource(selector, ins);
}
std::optional<std::uint32_t> readSource(const ShaderIRSource &source,
                                        const ShaderIRInputs &inputs) {
    switch (source.kind) {
    case ShaderIRSourceKind::scalarRegister:
        if (source.value < inputs.scalar.size()) return inputs.scalar[source.value];
        break;
    case ShaderIRSourceKind::vectorRegister:
        if (source.value < inputs.vector.size()) return inputs.vector[source.value];
        break;
    case ShaderIRSourceKind::immediateBits: return source.value;
    case ShaderIRSourceKind::none: break;
    }
    return std::nullopt;
}
float bitsFloat(std::uint32_t bits) {
    float result;
    std::memcpy(&result, &bits, sizeof(result));
    return result;
}
std::uint32_t floatBits(float number) {
    std::uint32_t bits;
    std::memcpy(&bits, &number, sizeof(bits));
    return bits;
}
bool scalarOp(ShaderIROp op) {
    return op >= ShaderIROp::scalarMovImmediate && op <= ShaderIROp::scalarAnd;
}
bool vectorOp(ShaderIROp op) {
    return op >= ShaderIROp::vectorMov && op <= ShaderIROp::vectorMulF32;
}
} // namespace

std::optional<ShaderIRProgram> lowerGCNToShaderIR(
    const GCNShaderTrace &decoded, std::string *error) {
    if (error) error->clear();
    if (!decoded.ended || decoded.instructions.empty() ||
        decoded.instructions.size() > maxInstructions ||
        decoded.totalWords == 0 || decoded.totalWords > 1024 ||
        decoded.checksum == 0) {
        fail(error, "Empty, nonterminating, or oversized shader trace");
        return std::nullopt;
    }
    ShaderIRProgram output;
    output.sourceChecksum = decoded.checksum;
    output.irChecksum = fnvOffset;
    const std::uint64_t base = decoded.instructions.front().guestAddress;
    if ((base & 3u) != 0 ||
        base > std::numeric_limits<std::uint64_t>::max() -
                   (static_cast<std::uint64_t>(decoded.totalWords) * 4 - 1)) {
        fail(error, "Unaligned or overflowing instruction address"); return std::nullopt;
    }
    std::uint32_t expectedOffset = 0;
    std::uint32_t scalarCount = 0, vectorCount = 0, scalarControlCount = 0;
    for (std::size_t i = 0; i < decoded.instructions.size(); ++i) {
        const GCNInstruction &ins = decoded.instructions[i];
        const bool literal = (ins.encoding == GCNEncoding::sop1 ||
                              ins.encoding == GCNEncoding::sop2 ||
                              ins.encoding == GCNEncoding::vop1 ||
                              ins.encoding == GCNEncoding::vop2) &&
                             (ins.src0 == 255 ||
                              (ins.encoding == GCNEncoding::sop2 && ins.src1 == 255));
        if (ins.wordOffset != expectedOffset ||
            ins.guestAddress != base + static_cast<std::uint64_t>(expectedOffset) * 4 ||
            ins.wordCount != (ins.hasLiteral ? 2u : 1u) ||
            ins.hasLiteral != literal ||
            expectedOffset >= decoded.totalWords ||
            ins.wordCount > decoded.totalWords - expectedOffset) {
            fail(error, "Invalid GCN instruction span or literal metadata");
            return std::nullopt;
        }
        ShaderIRInstruction ir;
        ir.guestAddress = ins.guestAddress;
        ir.wordOffset = ins.wordOffset;
        ir.destination = ins.dst;
        std::optional<ShaderIRSource> a, b;
        switch (ins.encoding) {
        case GCNEncoding::sopk:
            if (ins.opcode != 0 || ins.dst > 101 || ins.src0 > 0xFFFF || ins.hasLiteral)
                break;
            ir.op = ShaderIROp::scalarMovImmediate;
            ir.a = {ShaderIRSourceKind::immediateBits, signExtend16(ins.src0)};
            break;
        case GCNEncoding::sop1:
            if (ins.dst > 101 || (ins.opcode != 0 && ins.opcode != 4)) break;
            a = scalarSource(ins.src0, ins);
            if (!a) break;
            ir.op = ins.opcode == 0 ? ShaderIROp::scalarMov : ShaderIROp::scalarNot;
            ir.a = *a;
            break;
        case GCNEncoding::sop2:
            if (ins.dst > 101 || (ins.opcode != 0 && ins.opcode != 1 && ins.opcode != 12) ||
                (ins.src0 == 255 && ins.src1 == 255)) break;
            a = scalarSource(ins.src0, ins);
            b = scalarSource(ins.src1, ins);
            if (!a || !b) break;
            ir.op = ins.opcode == 0 ? ShaderIROp::scalarAdd :
                    ins.opcode == 1 ? ShaderIROp::scalarSub : ShaderIROp::scalarAnd;
            ir.a = *a; ir.b = *b;
            break;
        case GCNEncoding::vop1:
            if (ins.opcode != 1 || ins.dst > 255) break;
            a = vectorSource(ins.src0, ins);
            if (!a) break;
            ir.op = ShaderIROp::vectorMov;
            ir.a = *a;
            break;
        case GCNEncoding::vop2:
            if (ins.opcode != 1 && ins.opcode != 5) break;
            a = vectorSource(ins.src0, ins);
            if (!a || ins.src1 > 255) break;
            ir.op = ins.opcode == 1 ? ShaderIROp::vectorAddF32 : ShaderIROp::vectorMulF32;
            ir.a = *a;
            ir.b = {ShaderIRSourceKind::vectorRegister, ins.src1};
            break;
        case GCNEncoding::sopp:
            if (ins.hasLiteral || ins.opcode > 1 || (ins.opcode == 0 && ins.src0 > 15) ||
                (ins.opcode == 1 && ins.src0 != 0)) break;
            ir.op = ins.opcode == 0 ? ShaderIROp::nop : ShaderIROp::end;
            break;
        }
        if ((ins.encoding == GCNEncoding::sopp &&
             (ins.hasLiteral || ins.opcode > 1 ||
              (ins.opcode == 0 && ins.src0 > 15) ||
              (ins.opcode == 1 && ins.src0 != 0))) ||
            (ir.op == ShaderIROp::nop && ins.encoding != GCNEncoding::sopp) ||
            (ir.op == ShaderIROp::end && (ins.encoding != GCNEncoding::sopp ||
                                          i != decoded.instructions.size() - 1)) ||
            (i == decoded.instructions.size() - 1 && ir.op != ShaderIROp::end)) {
            fail(error, "Unsupported opcode or malformed termination");
            return std::nullopt;
        }
        if (scalarOp(ir.op)) ++scalarCount;
        if (vectorOp(ir.op)) ++vectorCount;
        if (ir.op == ShaderIROp::nop || ir.op == ShaderIROp::end) ++scalarControlCount;
        hash32(output.irChecksum, static_cast<std::uint32_t>(ir.op));
        hash64(output.irChecksum, ir.guestAddress);
        hash32(output.irChecksum, ir.wordOffset);
        hash32(output.irChecksum, ir.destination);
        hash32(output.irChecksum, static_cast<std::uint32_t>(ir.a.kind));
        hash32(output.irChecksum, ir.a.value);
        hash32(output.irChecksum, static_cast<std::uint32_t>(ir.b.kind));
        hash32(output.irChecksum, ir.b.value);
        output.instructions.push_back(ir);
        expectedOffset += ins.wordCount;
    }
    // Scalar counts from ISA include scalar NOP and S_ENDPGM.
    if (expectedOffset != decoded.totalWords ||
        scalarCount + scalarControlCount != decoded.scalarInstructions ||
        vectorCount != decoded.vectorInstructions ||
        output.instructions.back().op != ShaderIROp::end) {
        fail(error, "Shader trace counters or termination inconsistent");
        return std::nullopt;
    }
    output.scalarInstructions = decoded.scalarInstructions;
    output.vectorInstructions = decoded.vectorInstructions;
    return output;
}

std::optional<ShaderIRExecution> evaluateShaderIR(
    const ShaderIRProgram &program, const ShaderIRInputs &inputs,
    std::string *error) {
    if (error) error->clear();
    if (program.instructions.empty() || program.instructions.size() > maxInstructions ||
        program.irChecksum == 0 || program.sourceChecksum == 0 ||
        program.instructions.back().op != ShaderIROp::end) {
        fail(error, "Bad IR program"); return std::nullopt;
    }
    ShaderIRExecution result;
    result.finalRegisters = inputs;
    for (std::size_t i = 0; i < program.instructions.size(); ++i) {
        const auto &ins = program.instructions[i];
        if (ins.op == ShaderIROp::end) {
            if (i != program.instructions.size() - 1) {
                fail(error, "End instruction before end of IR program"); return std::nullopt;
            }
            ++result.executedInstructions;
            result.terminated = true;
            continue;
        }
        if (ins.op == ShaderIROp::nop) {
            ++result.executedInstructions;
            continue;
        }
        const auto a = readSource(ins.a, result.finalRegisters);
        if (!a || (scalarOp(ins.op) && ins.destination >= result.finalRegisters.scalar.size()) ||
            (vectorOp(ins.op) && ins.destination >= result.finalRegisters.vector.size())) {
            fail(error, "Invalid register operand or destination"); return std::nullopt;
        }
        std::uint32_t value = 0;
        switch (ins.op) {
        case ShaderIROp::scalarMovImmediate:
        case ShaderIROp::scalarMov:
        case ShaderIROp::vectorMov:
            value = *a; break;
        case ShaderIROp::scalarNot: value = ~*a; break;
        case ShaderIROp::scalarAdd:
        case ShaderIROp::scalarSub:
        case ShaderIROp::scalarAnd: {
            const auto b = readSource(ins.b, result.finalRegisters);
            if (!b) { fail(error, "Invalid scalar source"); return std::nullopt; }
            value = ins.op == ShaderIROp::scalarAdd ? (*a + *b) :
                    ins.op == ShaderIROp::scalarSub ? (*a - *b) : (*a & *b);
            break;
        }
        case ShaderIROp::vectorAddF32:
        case ShaderIROp::vectorMulF32: {
            const auto b = readSource(ins.b, result.finalRegisters);
            if (!b) { fail(error, "Invalid vector source"); return std::nullopt; }
            const float x = bitsFloat(*a), y = bitsFloat(*b);
            if (!std::isfinite(x) || !std::isfinite(y)) {
                fail(error, "Nonfinite vector input outside diagnostic subset"); return std::nullopt;
            }
            const float f = ins.op == ShaderIROp::vectorAddF32 ? x + y : x * y;
            if (!std::isfinite(f)) {
                fail(error, "Nonfinite vector output outside diagnostic subset"); return std::nullopt;
            }
            value = floatBits(f);
            break;
        }
        default:
            fail(error, "Unknown IR opcode"); return std::nullopt;
        }
        if (scalarOp(ins.op)) {
            result.finalRegisters.scalar[ins.destination] = value;
            ++result.scalarWrites;
        } else if (vectorOp(ins.op)) {
            result.finalRegisters.vector[ins.destination] = value;
            ++result.vectorWrites;
        } else {
            fail(error, "Invalid IR instruction class"); return std::nullopt;
        }
        ++result.executedInstructions;
    }
    if (!result.terminated) { fail(error, "IR program did not terminate"); return std::nullopt; }
    result.resultChecksum = fnvOffset;
    hash64(result.resultChecksum, program.irChecksum);
    hash32(result.resultChecksum, result.executedInstructions);
    hash32(result.resultChecksum, result.scalarWrites);
    hash32(result.resultChecksum, result.vectorWrites);
    for (const auto value : result.finalRegisters.scalar) hash32(result.resultChecksum, value);
    for (const auto value : result.finalRegisters.vector) hash32(result.resultChecksum, value);
    return result;
}

std::optional<ShaderIRDiagnostic> runShaderIRDiagnostic() {
    try {
        const auto previous = runGCNShaderDiagnostic();
        if (!previous || !previous->shaderReadOnly || !previous->descriptorReadOnly ||
            !previous->buffer.supportedLinearMetadata) return std::nullopt;
        std::string error;
        const auto ir = lowerGCNToShaderIR(previous->shader, &error);
        if (!ir) return std::nullopt;
        ShaderIRInputs inputs;
        inputs.vector[1] = 0x40000000u; // 2.0f in a test VGPR
        const auto execution = evaluateShaderIR(*ir, inputs, &error);
        if (!execution) return std::nullopt;
        ShaderIRDiagnostic result;
        result.program = *ir;
        result.execution = *execution;
        result.pm4Packets = previous->pm4Packets;
        result.pm4Draws = previous->pm4Draws;
        result.shaderAddress = previous->pixelShaderAddress;
        result.sourceReadOnly = previous->shaderReadOnly;
        result.descriptorReadOnly = previous->descriptorReadOnly;
        return result;
    } catch (...) { return std::nullopt; }
}

} // namespace misaki
