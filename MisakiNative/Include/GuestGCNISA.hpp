#pragma once

#include "GuestPM4State.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace misaki {

// Restricted, metadata-only interpretation of documented AMD GCN ISA layouts.
// No ALU execution, instruction scheduling, shader translation, Sony binaries,
// GNM/GNMX behavior, or host GPU access.
enum class GCNEncoding : std::uint32_t {
    sop2 = 1, sop1 = 2, sopk = 3, sopp = 4, vop2 = 5, vop1 = 6
};

struct GCNInstruction {
    std::uint64_t guestAddress = 0;
    std::uint32_t wordOffset = 0;
    std::uint32_t wordCount = 0;
    GCNEncoding encoding = GCNEncoding::sopp;
    std::uint32_t opcode = 0;
    std::uint32_t dst = 0;
    std::uint32_t src0 = 0;
    std::uint32_t src1 = 0;
    std::uint32_t literal = 0;
    bool hasLiteral = false;
    std::string mnemonic;
};

struct GCNShaderTrace {
    std::vector<GCNInstruction> instructions;
    std::uint32_t scalarInstructions = 0;
    std::uint32_t vectorInstructions = 0;
    std::uint32_t literalWords = 0;
    std::uint32_t totalWords = 0;
    bool ended = false;
    std::uint64_t checksum = 0;
};

// Consumes an EXACT shader range and requires S_ENDPGM as the final instruction.
// Unsupported formats/opcodes or incomplete literals fail atomically.
std::optional<GCNShaderTrace> decodeGCNShaderWords(
    const std::uint32_t *words, std::size_t count, std::uint64_t guestAddress,
    std::string *error = nullptr);
std::optional<GCNShaderTrace> decodeGuestGCNShader(
    const GuestMemory &memory, std::uint64_t address, std::size_t count,
    std::string *error = nullptr);

// 128-bit GCN buffer-resource descriptor metadata (GCN3-style 48-bit address,
// 14-bit stride). Raw fields are not evidence of a mapped or usable resource.
struct GCNBufferDescriptor {
    std::uint64_t baseAddress = 0;
    std::uint32_t strideBytes = 0;
    std::uint32_t records = 0;
    std::array<std::uint32_t, 4> channelSelectors{};
    std::uint32_t numericFormat = 0;
    std::uint32_t dataFormat = 0;
    std::uint32_t resourceType = 0;
    bool swizzleEnabled = false;
    bool addThreadID = false;
    bool supportedLinearMetadata = false;
};
GCNBufferDescriptor decodeGCNBufferWords(const std::array<std::uint32_t, 4> &words);
std::optional<GCNBufferDescriptor> decodeGuestGCNBuffer(
    const GuestMemory &memory, std::uint64_t address);

struct GCNShaderDiagnostic {
    GCNShaderTrace shader;
    GCNBufferDescriptor buffer;
    std::uint32_t pm4Packets = 0;
    std::uint32_t pm4Draws = 0;
    std::uint64_t pixelShaderAddress = 0;
    bool shaderReadOnly = false;
    bool descriptorReadOnly = false;
};

// Original GCN instruction fixtures deliberately placed at the pixel-shader
// address extracted from the *real-form*, self-authored PM4 M24 fixture.
std::vector<std::uint32_t> makeGCNShaderFixture();
std::array<std::uint32_t, 4> makeGCNBufferFixture();
std::optional<GCNShaderDiagnostic> runGCNShaderDiagnostic();

} // namespace misaki
