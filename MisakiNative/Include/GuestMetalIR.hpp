#pragma once

#include "GuestShaderIR.hpp"

#include <array>
#include <cstdint>
#include <optional>
#include <string>

namespace misaki {

// Milestone 27: converts the already-validated, single-lane toy GCN IR into
// Metal Shading Language COMPUTE source. Not an AMD wavefront translator,
// PS4 shader pipeline, or graphics rasterization implementation.
struct MetalIRTranslation {
    std::string source;
    std::array<std::uint32_t, 6> expected{}; // s1 s2 s3 v0 v1 v2, raw u32 bits
    std::uint32_t emittedALU = 0;
    std::uint32_t sourceBytes = 0; // includes terminal NUL for C ABI
    std::uint64_t irChecksum = 0;
    std::uint64_t sourceHash = 0;
    std::uint64_t softwareResultChecksum = 0;
};

// Refuses unsupported or malformed programs, nonfinite float arithmetic, and
// oversized output. Text is GENERATED only from typed opcodes, never guest text.
std::optional<MetalIRTranslation> translateShaderIRToMetal(
    const ShaderIRProgram &program, const ShaderIRInputs &inputs,
    std::string *error = nullptr);

// Integrated PM4 -> GCN ISA -> IR -> MSL pipeline. The Metal source is
// compiled and run only by the iOS layer; the C++ diagnostic runs no GPU code.
std::optional<MetalIRTranslation> runMetalIRDiagnostic();

} // namespace misaki
