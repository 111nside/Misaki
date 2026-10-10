#pragma once

#include "GuestPM4Indirect.hpp"

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace misaki {

// Register-state interpretation for one carefully bounded GCN-era subset.
// A 'complete' snapshot means metadata present and internally consistent,
// NOT that the surface is mapped, PS4-compatible, or GPU-renderable.
enum class PM4DrawStatus : std::uint32_t {
    metadataReady = 0,
    missingRegisters = 1,
    disabledTarget = 2,
    invalidScissor = 3,
    invalidAddress = 4,
    unsupportedTargets = 5,
    unsupportedFormat = 6,
};

struct PM4DrawSnapshot {
    std::uint64_t packetAddress = 0;
    std::uint64_t colorAddress = 0;    // CB_COLOR0_BASE units of 256 bytes
    std::uint64_t pixelShaderAddress = 0; // 256-byte aligned program address
    std::uint32_t packetIndex = 0;
    std::uint32_t depth = 0;
    std::uint32_t vertexCount = 0;
    std::uint32_t scissorX = 0;
    std::uint32_t scissorY = 0;
    std::uint32_t scissorWidth = 0;
    std::uint32_t scissorHeight = 0;
    std::uint32_t pitchTileMax = 0; // raw bitfield, NOT surface width
    std::uint32_t colorFormatField = 0; // raw bitfield, NOT host pixel format
    std::uint32_t targetMask = 0;
    std::uint32_t observedMask = 0;
    PM4DrawStatus status = PM4DrawStatus::missingRegisters;
};

struct PM4StateTrace {
    std::vector<PM4DrawSnapshot> draws;
    std::uint32_t packets = 0;
    std::uint32_t indirectBuffers = 0;
    std::uint32_t registerWrites = 0;
    std::uint32_t readyDraws = 0;
    std::uint32_t rejectedDraws = 0;
    std::uint64_t checksum = 0;
};

// Reuses the validated recursive decoder. Replays register updates *in packet
// execution order* and snapshots active state at every DRAW_INDEX_AUTO.
// Values are read-only; no guest shader or GPU surface is dereferenced.
std::optional<PM4StateTrace> analyzePM4DrawStates(
    const GuestMemory &memory, std::uint64_t address, std::size_t words,
    std::string *error = nullptr);

// A complete one-draw metadata fixture from Milestone 23.
std::optional<PM4StateTrace> runPM4StateDiagnostic();

} // namespace misaki
