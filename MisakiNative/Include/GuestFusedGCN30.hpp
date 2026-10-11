#pragma once

#include "GuestMUBUF.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace misaki {

// Milestone 30: one restricted, single-lane program with interleaved original-
// format GCN1.1 scalar/vector ALU and BUFFER_LOAD/STORE_DWORD instructions.
// This is NOT general shader execution, synchronization, PS4 firmware, or GCN
// wavefront semantics. Unknown encodings and addressing modes are rejected.
enum class FusedGCN30Kind : std::uint32_t { alu = 1, memory = 2, end = 3 };
struct FusedGCN30Step {
    FusedGCN30Kind kind = FusedGCN30Kind::alu;
    std::uint32_t wordOffset = 0;
    ShaderIRInstruction alu{};
    MUBUFInstruction memory{};
};
struct FusedGCN30Program {
    std::vector<FusedGCN30Step> steps;
    std::uint32_t codeWords = 0;
    std::uint32_t aluOperations = 0;
    std::uint32_t bufferLoads = 0;
    std::uint32_t bufferStores = 0;
    std::uint64_t checksum = 0;
};

std::optional<FusedGCN30Program> decodeFusedGCN30(
    const std::uint32_t *words, std::size_t count,
    std::uint64_t address, std::string *error = nullptr);
std::optional<FusedGCN30Program> decodeGuestFusedGCN30(
    const GuestMemory &memory, std::uint64_t address,
    std::size_t count, std::string *error = nullptr);

// CPU reference runs the same ordered instructions using bounded SGPR/VGPR
// state and isolated read/write guest-memory regions. One store is committed
// only after all its operands and permissions validate.
struct FusedGCN30Reference {
    std::array<std::uint32_t, 8> registers{}; // s1,s2,s3,s10,s12,v4,v8,v9
    std::array<std::uint32_t, 64> output{};
    std::uint32_t executed = 0;
    std::uint32_t index = 0;
    std::uint32_t loaded = 0;
    std::uint32_t stored = 0;
};
std::optional<FusedGCN30Reference> executeFusedGCN30(
    const FusedGCN30Program &program, GuestMemory &memory,
    std::uint64_t inputDescriptor, std::uint64_t outputDescriptor,
    const std::array<std::uint32_t, 64> &initial,
    std::string *error = nullptr);

std::optional<std::string> translateFusedGCN30ToMetal(
    const FusedGCN30Program &program,
    const GCNBufferDescriptor &input, const GCNBufferDescriptor &output,
    std::string *error = nullptr);

struct FusedGCN30Plan {
    FusedGCN30Program decoded;
    std::string metalSource;
    std::array<std::uint8_t, 1024> inputBytes{};
    std::array<std::uint32_t, 64> initial{};
    FusedGCN30Reference expected;
    std::uint64_t inputBase = 0;
    std::uint64_t outputBase = 0;
    std::uint64_t metalSourceHash = 0;
    std::uint64_t expectedHash = 0;
    bool codeReadOnly = false;
    bool descriptorsReadOnly = false;
    bool inputReadOnly = false;
    bool outputWritable = false;
};

std::vector<std::uint32_t> makeFusedGCN30Fixture();
std::optional<FusedGCN30Plan> runFusedGCN30Diagnostic();
} // namespace misaki
