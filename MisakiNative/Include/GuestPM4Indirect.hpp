#pragma once

#include "GuestPM4.hpp"

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace misaki {

// Milestone 23: bounded AMD PM4 INDIRECT_BUFFER decoding with guest-memory
// permission checks. This is metadata inspection, NEVER GPU command execution.
namespace pm4_indirect_opcode {
constexpr std::uint8_t indirectBuffer = 0x3F;
constexpr std::uint8_t indirectBufferConst = 0x33;
}

struct PM4IndirectPacket {
    std::uint64_t guestAddress = 0; // address of PM4 header, in guest memory
    std::uint32_t type = 0;
    std::uint32_t opcode = 0;
    std::uint32_t depth = 0;
    std::uint32_t bodyWords = 0;
};

// Only raw GCN register values are tracked; their meanings vary by GPU revision.
// No validation of pitch, base, shader binary, image formats, or renderability.
struct PM4RawRenderState {
    std::uint32_t color0Base = 0;    // context register 0xA318
    std::uint32_t color0Pitch = 0;   // context register 0xA319
    std::uint32_t color0Info = 0;    // context register 0xA31C
    std::uint32_t targetMask = 0;    // context register 0xA08E
    std::uint32_t scissorTL = 0;     // context register 0xA080
    std::uint32_t scissorBR = 0;     // context register 0xA081
    std::uint32_t pixelShaderLow = 0;  // SH register 0x2C08
    std::uint32_t pixelShaderHigh = 0; // SH register 0x2C09
    std::uint32_t observedMask = 0; // bit i = corresponding register observed
    bool hasRenderTargetMetadata() const {
        return (observedMask & 0x0Fu) == 0x0Fu;
    }
};

struct PM4IndirectTrace {
    std::vector<PM4IndirectPacket> packets; // execution order, child packets inline
    std::vector<PM4RegisterWrite> registers;
    PM4RawRenderState state;
    std::uint32_t indirectBuffers = 0;
    std::uint32_t indirectConstBuffers = 0;
    std::uint32_t maxDepth = 0;
    std::uint32_t drawAuto = 0;
    std::uint32_t eventWrites = 0;
    std::uint32_t totalWords = 0;
    std::uint32_t registerWrites = 0;
    std::uint64_t checksum = 0;
};

// Following only ordinary 3-DWORD INDIRECT_BUFFER and INDIRECT_BUFFER_CONST
// payloads, with strict subsets of their control bits. Predication, chaining,
// guest GPU page tables, shader execution, and unknown opcodes fail closed.
// Prevents recursive cycles, depth bombs, oversized buffers, and partial traces.
std::optional<PM4IndirectTrace> decodePM4Indirect(const GuestMemory &guest,
                                                   std::uint64_t address,
                                                   std::size_t wordCount,
                                                   std::string *error = nullptr);

// Synthetic PM4 sequences formatted using AMD's real packet header fields.
// These are not captures from PS4 firmware or commercially distributed games.
struct PM4IndirectFixture {
    std::uint64_t root = 0x40000;
    std::uint64_t child = 0x50000;
    std::uint64_t grandchild = 0x60000;
    std::vector<std::uint32_t> rootWords;
    std::vector<std::uint32_t> childWords;
    std::vector<std::uint32_t> grandchildWords;
};
PM4IndirectFixture makePM4IndirectFixture();
bool installPM4IndirectFixture(GuestMemory &memory, const PM4IndirectFixture &fixture);

} // namespace misaki
