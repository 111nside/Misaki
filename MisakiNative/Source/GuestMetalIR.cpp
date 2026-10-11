#include "../Include/GuestMetalIR.hpp"

#include <limits>
#include <sstream>
#include <utility>

namespace misaki {
namespace {
constexpr std::uint64_t fnvOffset = 14695981039346656037ULL;
constexpr std::uint64_t fnvPrime = 1099511628211ULL;
constexpr std::size_t maxSourceBytes = 32768;

void fail(std::string *message, const char *reason) {
    if (message) *message = reason;
}

std::string hex32(std::uint32_t value) {
    static constexpr char digits[] = "0123456789ABCDEF";
    std::string s = "0x00000000u";
    for (unsigned digit = 0; digit < 8; ++digit)
        s[2 + digit] = digits[(value >> ((7 - digit) * 4)) & 15u];
    return s;
}

std::optional<std::string> source(const ShaderIRSource &src) {
    switch (src.kind) {
    case ShaderIRSourceKind::scalarRegister:
        if (src.value < 102) return "s[" + std::to_string(src.value) + "]";
        break;
    case ShaderIRSourceKind::vectorRegister:
        if (src.value < 256) return "v[" + std::to_string(src.value) + "]";
        break;
    case ShaderIRSourceKind::immediateBits: return hex32(src.value);
    case ShaderIRSourceKind::none: break;
    }
    return std::nullopt;
}

bool scalarOp(ShaderIROp op) {
    return op >= ShaderIROp::scalarMovImmediate && op <= ShaderIROp::scalarAnd;
}
bool vectorOp(ShaderIROp op) {
    return op >= ShaderIROp::vectorMov && op <= ShaderIROp::vectorMulF32;
}
std::uint64_t hash(const std::string &text) {
    std::uint64_t h = fnvOffset;
    for (const auto ch : text) {
        h ^= static_cast<unsigned char>(ch);
        h *= fnvPrime;
    }
    return h;
}
}

std::optional<MetalIRTranslation> translateShaderIRToMetal(
    const ShaderIRProgram &program, const ShaderIRInputs &inputs,
    std::string *error) {
    if (error) error->clear();
    if (program.instructions.empty() || program.instructions.size() > 256 ||
        program.irChecksum == 0 || program.sourceChecksum == 0 ||
        program.instructions.back().op != ShaderIROp::end) {
        fail(error, "Missing, oversized, or nonterminating shader IR");
        return std::nullopt;
    }
    // Explicitly require the existing software reference to accept ALL operands
    // before generating text; also supplies the expected GPU comparison values.
    const auto reference = evaluateShaderIR(program, inputs, error);
    if (!reference || !reference->terminated) {
        if (error && error->empty()) fail(error, "Reference evaluation rejected IR");
        return std::nullopt;
    }

    std::ostringstream msl;
    msl << "#include <metal_stdlib>\n"
           "using namespace metal;\n"
           "kernel void misaki_ir27(device uint *outValues [[buffer(0)]], "
           "uint threadID [[thread_position_in_grid]]) {\n"
           "    if (threadID != 0u) return;\n"
           "    uint s[102] = {};\n"
           "    uint v[256] = {};\n";
    for (std::size_t i = 0; i < inputs.scalar.size(); ++i)
        if (inputs.scalar[i] != 0)
            msl << "    s[" << i << "] = " << hex32(inputs.scalar[i]) << ";\n";
    for (std::size_t i = 0; i < inputs.vector.size(); ++i)
        if (inputs.vector[i] != 0)
            msl << "    v[" << i << "] = " << hex32(inputs.vector[i]) << ";\n";

    std::uint32_t statements = 0;
    for (std::size_t i = 0; i < program.instructions.size(); ++i) {
        const auto &ins = program.instructions[i];
        if (ins.op == ShaderIROp::end) {
            if (i != program.instructions.size() - 1) {
                fail(error, "Early shader end"); return std::nullopt;
            }
            continue;
        }
        if (ins.op == ShaderIROp::nop) continue;
        if (!scalarOp(ins.op) && !vectorOp(ins.op)) {
            fail(error, "Unsupported IR opcode"); return std::nullopt;
        }
        const auto a = source(ins.a);
        if (!a || (scalarOp(ins.op) && ins.destination >= 102) ||
            (vectorOp(ins.op) && ins.destination >= 256)) {
            fail(error, "Invalid destination or first operand"); return std::nullopt;
        }
        const std::string destination = std::string(scalarOp(ins.op) ? "s[" : "v[") +
                                        std::to_string(ins.destination) + "]";
        std::string expression;
        switch (ins.op) {
        case ShaderIROp::scalarMovImmediate:
        case ShaderIROp::scalarMov:
        case ShaderIROp::vectorMov:
            expression = *a; break;
        case ShaderIROp::scalarNot: expression = "(~" + *a + ")"; break;
        case ShaderIROp::scalarAdd:
        case ShaderIROp::scalarSub:
        case ShaderIROp::scalarAnd:
        case ShaderIROp::vectorAddF32:
        case ShaderIROp::vectorMulF32: {
            const auto b = source(ins.b);
            if (!b) { fail(error, "Invalid second operand"); return std::nullopt; }
            switch (ins.op) {
            case ShaderIROp::scalarAdd: expression = "(" + *a + " + " + *b + ")"; break;
            case ShaderIROp::scalarSub: expression = "(" + *a + " - " + *b + ")"; break;
            case ShaderIROp::scalarAnd: expression = "(" + *a + " & " + *b + ")"; break;
            case ShaderIROp::vectorAddF32:
            case ShaderIROp::vectorMulF32: {
                const char *op = ins.op == ShaderIROp::vectorAddF32 ? " + " : " * ";
                expression = "as_type<uint>(as_type<float>(" + *a + ")" + op +
                             "as_type<float>(" + *b + "))";
                break;
            }
            default: break;
            }
            break;
        }
        default: fail(error, "Unsupported IR opcode"); return std::nullopt;
        }
        msl << "    " << destination << " = " << expression << ";\n";
        ++statements;
    }
    msl << "    outValues[0] = s[1];\n"
           "    outValues[1] = s[2];\n"
           "    outValues[2] = s[3];\n"
           "    outValues[3] = v[0];\n"
           "    outValues[4] = v[1];\n"
           "    outValues[5] = v[2];\n"
           "}\n";
    MetalIRTranslation translation;
    translation.source = msl.str();
    if (translation.source.size() + 1 > maxSourceBytes ||
        translation.source.size() + 1 > std::numeric_limits<std::uint32_t>::max()) {
        fail(error, "MSL shader source exceeds byte budget"); return std::nullopt;
    }
    translation.emittedALU = statements;
    translation.sourceBytes = static_cast<std::uint32_t>(translation.source.size() + 1);
    translation.irChecksum = program.irChecksum;
    translation.sourceHash = hash(translation.source);
    translation.softwareResultChecksum = reference->resultChecksum;
    translation.expected = {
        reference->finalRegisters.scalar[1], reference->finalRegisters.scalar[2],
        reference->finalRegisters.scalar[3], reference->finalRegisters.vector[0],
        reference->finalRegisters.vector[1], reference->finalRegisters.vector[2]
    };
    return translation;
}

std::optional<MetalIRTranslation> runMetalIRDiagnostic() {
    try {
        const auto diagnostic = runShaderIRDiagnostic();
        if (!diagnostic || !diagnostic->sourceReadOnly ||
            !diagnostic->descriptorReadOnly || diagnostic->pm4Packets != 11 ||
            diagnostic->pm4Draws != 1) return std::nullopt;
        ShaderIRInputs inputs;
        inputs.vector[1] = 0x40000000u;
        auto translated = translateShaderIRToMetal(diagnostic->program, inputs);
        if (!translated || translated->softwareResultChecksum !=
                           diagnostic->execution.resultChecksum)
            return std::nullopt;
        return translated;
    } catch (...) { return std::nullopt; }
}

} // namespace misaki
