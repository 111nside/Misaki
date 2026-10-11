#pragma once
#include "GuestBootReadiness.hpp"
#include "GuestGCNMemoryStage.hpp"
#include "GuestSCEBinding.hpp"
#include "GuestSCEKernel.hpp"
#include "GuestSCEStaging.hpp"
#include <array>
#include <cstdint>
#include <optional>
namespace misaki {
// Integrated 41-45 demonstration with original fixture data. This is not
// retail SELF support, real libSce HLE, hardware GCN, or a firmware boot.
struct PS4PhaseTwoDiagnostic {
    std::array<bool,5> stagePassed{};
    std::uint32_t mappedSegments=0, relocations=0, imports=0;
    std::uint32_t serviceCalls=0, guestInstructions=0, emittedBytes=0;
    std::uint64_t guestResult=0, relocationValue=0;
    std::uint32_t pm4Packets=0, gpuPackets=0, sprites=0;
    std::uint64_t completedFence=0, framebufferChecksum=0;
    std::uint32_t bootBlockers=0;
    bool firmwareAttempted=false, homeMenuBooted=false;
};
std::optional<PS4PhaseTwoDiagnostic> runPS4PhaseTwoDiagnostic();
} // namespace misaki
