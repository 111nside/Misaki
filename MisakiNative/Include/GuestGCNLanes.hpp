#pragma once
#include "GuestFusedGCN30.hpp"
#include <array>
#include <cstdint>
#include <optional>

namespace misaki {
// A cooperative four-lane, scalar reference for the existing restricted fused
// GCN shader. This is NOT real 64-lane GCN wavefront or divergent execution.
struct GCNLane38Result {
    std::uint32_t activeMask = 0;
    std::uint32_t executedLanes = 0;
    std::uint32_t instructions = 0;
    std::array<std::uint32_t, 4> indices{};
    std::array<std::uint32_t, 4> loaded{};
    std::array<std::uint32_t, 4> stored{};
    std::array<std::uint32_t, 64> output{};
    bool codeProtected = false;
    bool descriptorsProtected = false;
    bool inputProtected = false;
    std::uint64_t checksum = 0;
};
std::optional<GCNLane38Result> runGCNLaneMemoryDiagnostic(std::uint32_t activeMask = 15);
}
