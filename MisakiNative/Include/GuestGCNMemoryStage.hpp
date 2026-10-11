#pragma once
#include "GuestGraphics21.hpp"
#include "GuestIntegratedRuntime.hpp"
#include "GuestPM4State.hpp"
#include <cstdint>
#include <optional>
#include <string>
namespace misaki {
// A safely bounded, software-rasterized staging path. PM4 supplies candidate
// target/shader addresses; the existing Misaki 2D protocol creates the pixels.
// This does NOT rasterize AMD PM4 draws or execute a genuine PS4 pixel shader.
struct GCNMemoryStageReport {
    std::uint32_t pm4Packets = 0;
    std::uint32_t gpuPackets = 0;
    std::uint32_t sprites = 0;
    std::uint64_t targetAddress = 0;
    std::uint64_t shaderAddress = 0;
    std::uint64_t submittedFence = 0;
    std::uint64_t completedFence = 0;
    std::uint64_t framebufferChecksum = 0;
    bool targetMappedWritable = false;
    bool shaderExecutable = false;
};
bool installGCNMemoryStageFixture(GuestMemory &memory);
std::optional<GCNMemoryStageReport> executeGCNMemoryStage(
    GuestMemory &memory, std::uint64_t pm4Root, std::size_t pm4Words,
    std::uint64_t commandAddress, std::uint32_t commands, std::uint64_t fence,
    std::string *error = nullptr);
} // namespace misaki
