#pragma once

#include "GuestGCNISA.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace misaki {

// Milestone 26: safe, single-lane diagnostic IR for a *strict subset* of
// original-format GCN scalar/vector ALU instructions. NOT wavefront execution,
// an AMD-compatible GPU, a Metal shader translator, or a PS4 runtime.
enum class ShaderIROp : std::uint32_t {
    scalarMovImmediate = 1, scalarMov = 2, scalarNot = 3,
    scalarAdd = 4, scalarSub = 5, scalarAnd = 6,
    vectorMov = 7, vectorAddF32 = 8, vectorMulF32 = 9,
    nop = 10, end = 11
};

enum class ShaderIRSourceKind : std::uint32_t {
    none = 0, scalarRegister = 1, vectorRegister = 2, immediateBits = 3
};

struct ShaderIRSource {
    ShaderIRSourceKind kind = ShaderIRSourceKind::none;
    std::uint32_t value = 0; // register index, otherwise unmodified 32-bit bits
};

struct ShaderIRInstruction {
    ShaderIROp op = ShaderIROp::nop;
    std::uint64_t guestAddress = 0;
    std::uint32_t wordOffset = 0;
    std::uint32_t destination = 0;
    ShaderIRSource a;
    ShaderIRSource b;
};

struct ShaderIRProgram {
    std::vector<ShaderIRInstruction> instructions;
    std::uint32_t scalarInstructions = 0;
    std::uint32_t vectorInstructions = 0;
    std::uint64_t sourceChecksum = 0;
    std::uint64_t irChecksum = 0;
};

// A lowering pass, not just renamed GCN instruction records. Converts AMD
// operand selector encodings to typed SGPR/VGPR/immediate sources, including
// signed SOPK immediates and inline scalar constants. Rejects malformed traces.
std::optional<ShaderIRProgram> lowerGCNToShaderIR(
    const GCNShaderTrace &decoded, std::string *error = nullptr);

struct ShaderIRInputs {
    std::array<std::uint32_t, 102> scalar{};
    std::array<std::uint32_t, 256> vector{};
};

struct ShaderIRExecution {
    ShaderIRInputs finalRegisters;
    std::uint32_t executedInstructions = 0;
    std::uint32_t scalarWrites = 0;
    std::uint32_t vectorWrites = 0;
    bool terminated = false;
    std::uint64_t resultChecksum = 0;
};

// Software validation of *one virtual lane* with 32-bit integer and IEEE754
// float ALU; never executes arbitrary host code or touches host/guest GPU memory.
// NaN/Inf float inputs and outputs are rejected (this is stricter than GCN).
std::optional<ShaderIRExecution> evaluateShaderIR(
    const ShaderIRProgram &program, const ShaderIRInputs &inputs,
    std::string *error = nullptr);

struct ShaderIRDiagnostic {
    ShaderIRProgram program;
    ShaderIRExecution execution;
    std::uint32_t pm4Packets = 0;
    std::uint32_t pm4Draws = 0;
    std::uint64_t shaderAddress = 0;
    bool sourceReadOnly = false;
    bool descriptorReadOnly = false;
};

std::optional<ShaderIRDiagnostic> runShaderIRDiagnostic();

} // namespace misaki
