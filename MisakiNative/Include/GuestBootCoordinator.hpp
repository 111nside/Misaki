#pragma once
#include "GuestGCNLanes.hpp"
#include "GuestIntegratedRuntime.hpp"
#include "GuestPS4Formats.hpp"
#include "GuestPS4Imports.hpp"
#include <cstdint>
#include <optional>
#include <string>
#include <vector>
namespace misaki {
enum class BootCheckPhase : std::uint32_t {
    selfHeader = 36, syntheticImports = 37, shaderMemory = 38,
    integratedSession = 39, realFirmware = 40
};
struct BootCheckEvent {
    BootCheckPhase phase = BootCheckPhase::selfHeader;
    bool passed = false;
    std::string detail;
};
struct EarlyBootDiagnostic {
    std::vector<BootCheckEvent> events;
    PS4ExecutableMetadata executable;
    std::uint32_t resolvedImports = 0;
    std::uint32_t shaderLanes = 0;
    std::uint32_t shaderOperations = 0;
    std::uint32_t sessionProcesses = 0;
    std::uint32_t sessionInstructions = 0;
    std::uint32_t frameSprites = 0;
    std::uint32_t pm4Packets = 0;
    std::uint64_t lastFence = 0;
    bool syntheticDiagnosticsPassed = false;
    bool firmwareLaunched = false;
};
// Fail-closed integration diagnostic using self-authored fixtures only.
// This doesn't accept a system firmware image or invoke an actual PS4 boot path.
std::optional<EarlyBootDiagnostic> runEarlyBootDiagnostic();
}
