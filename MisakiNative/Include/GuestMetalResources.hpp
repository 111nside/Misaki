#pragma once

#include "GuestMetalIR.hpp"

#include <array>
#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace misaki {

// Milestone 28: a deliberately small, typed *diagnostic* resource stage.
// These operations are not decoded GCN MUBUF/MTBUF instructions. They show
// that an independently evaluated IR program can feed real Metal buffer IO.
enum class Resource28Opcode : std::uint32_t { loadU32 = 1, addRegister = 2, storeU32 = 3 };
struct Resource28Instruction {
    Resource28Opcode opcode = Resource28Opcode::loadU32;
    std::uint32_t referenceRegister = 0;  // 0..5 = s1,s2,s3,v0,v1,v2
    std::int32_t indexOffset = 0;         // applies only to load/store indices
};
struct Resource28Program {
    std::vector<Resource28Instruction> instructions;
};

struct Resource28Reference {
    std::array<std::uint32_t, 64> output{};
    std::uint32_t loadedWord = 0;
    std::uint32_t storedWord = 0;
    std::uint32_t readIndex = 0;
    std::uint32_t writeIndex = 0;
};

// GuestMemory enforces read permissions and the GCN descriptor supplies the
// record count and byte stride. The private fixture interprets the FIRST
// DWORD of each 16-byte record as raw uint32; this is NOT AMD format conversion.
std::optional<Resource28Reference> evaluateResource28(
    const Resource28Program &program, const GuestMemory &memory,
    const GCNBufferDescriptor &descriptor,
    const std::array<std::uint32_t, 6> &referenceRegisters,
    const std::array<std::uint32_t, 64> &initialOutput,
    std::string *error = nullptr);

struct MetalResource28Plan {
    std::string resourceMSL;
    std::array<std::uint8_t, 1024> guestBytes{};
    std::array<std::uint32_t, 64> outputInitial{};
    std::array<std::uint32_t, 64> outputExpected{};
    std::array<std::uint32_t, 6> registers{};
    std::uint64_t descriptorBase = 0;
    std::uint32_t descriptorStride = 0;
    std::uint32_t descriptorRecords = 0;
    std::uint32_t loadedWord = 0;
    std::uint32_t storedWord = 0;
    std::uint32_t readIndex = 0;
    std::uint32_t writeIndex = 0;
    std::uint64_t irChecksum = 0;
    std::uint64_t sourceHash = 0;
    std::uint64_t initialHash = 0;
    std::uint64_t expectedHash = 0;
    bool guestInputReadOnly = false;
    bool guestOutputWritable = false;
};

std::optional<std::string> translateResource28ToMetal(
    const Resource28Program &program, const GCNBufferDescriptor &descriptor,
    std::string *error = nullptr);

Resource28Program makeResource28Program();
std::optional<MetalResource28Plan> runMetalResource28Diagnostic();
} // namespace misaki
