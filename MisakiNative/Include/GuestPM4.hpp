#pragma once

#include "MisakiCore.hpp"

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace misaki {

// A bounded decoder for documented AMD Southern Islands/GCN PM4 packet
// framing and a small subset of PACKET3 opcodes. This is *not* Sony GNM,
// PS4-specific firmware behavior, command execution, or a GPU renderer.
namespace pm4_opcode {
constexpr std::uint8_t nop = 0x10;
constexpr std::uint8_t indexType = 0x2A;
constexpr std::uint8_t drawIndexAuto = 0x2D;
constexpr std::uint8_t numInstances = 0x2F;
constexpr std::uint8_t eventWrite = 0x46;
constexpr std::uint8_t setConfigReg = 0x68;
constexpr std::uint8_t setContextReg = 0x69;
constexpr std::uint8_t setShReg = 0x76;
constexpr std::uint8_t setUconfigReg = 0x79;
}

struct PM4RegisterWrite {
    std::uint32_t address = 0; // AMD register index (in DWORDs, not bytes).
    std::uint32_t value = 0;
    std::uint32_t packetIndex = 0;
    std::uint32_t shaderType = 0;
};

struct PM4PacketSummary {
    std::uint32_t type = 0;
    std::uint32_t opcode = 0; // 0 for type-0 and type-2 packets.
    std::uint32_t headerWord = 0;
    std::uint32_t bodyWords = 0;
};

struct PM4Trace {
    std::vector<PM4PacketSummary> packets;
    std::vector<PM4RegisterWrite> registers;
    std::uint32_t type0Packets = 0;
    std::uint32_t type2Packets = 0;
    std::uint32_t type3Packets = 0;
    std::uint32_t nops = 0;
    std::uint32_t contextWrites = 0;
    std::uint32_t shaderWrites = 0;
    std::uint32_t otherWrites = 0;
    std::uint32_t drawAuto = 0;
    std::uint32_t eventWrites = 0;
    std::uint32_t lastVertexCount = 0;
    std::uint64_t checksum = 0;
};

// Returns no partial trace on malformed, unsupported, predicated, or truncated
// input. There are no host-memory, GPU, file, or kernel side effects.
std::optional<PM4Trace> decodePM4(const std::uint32_t *words,
                                  std::size_t count,
                                  std::string *error = nullptr);

// Copies a bounded little-endian DWORD stream through guest read permissions.
// Does not follow INDIRECT_BUFFER pointers or trust addresses in the stream.
std::optional<PM4Trace> decodeGuestPM4(const GuestMemory &memory,
                                       std::uint64_t address,
                                       std::size_t words,
                                       std::string *error = nullptr);

// Self-authored example of real-form PM4 packets, NOT a Sony firmware capture.
std::vector<std::uint32_t> makePM4DiagnosticStream();

} // namespace misaki
