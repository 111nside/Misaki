#pragma once
#include "GuestBootCoordinator.hpp"
#include <cstdint>
#include <optional>
#include <string>
#include <vector>
namespace misaki {
struct PS4BootCapabilities {
    bool realSelfPayloadLoader = false;
    bool realSceKernelServices = false;
    bool comprehensiveGCNWavefront = false;
    bool ps4FirmwareImagesProvided = false;
    bool ps4FirmwareVirtualMemory = false;
};
struct PS4BootReadiness {
    std::uint32_t completedSyntheticChecks = 0;
    std::uint32_t totalChecks = 5;
    std::vector<std::string> blockers;
    bool prerequisitesReported = false;
    bool firmwareExecutionAttempted = false;
    bool homeMenuBooted = false;
};
// An evidence/reporting gate, NEVER a call to execute firmware.
PS4BootReadiness evaluatePS4BootReadiness(const EarlyBootDiagnostic &diagnostic,
                                          const PS4BootCapabilities &capabilities);
std::optional<PS4BootReadiness> runPS4BootReadinessDiagnostic();
}
