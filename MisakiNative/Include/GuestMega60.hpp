#pragma once
#include "GuestSCE56.hpp"
#include "GuestServices57.hpp"
#include "GuestGPUMemory58.hpp"
#include "GuestPersistent59.hpp"
#include "GuestCompatibility60.hpp"
#include <array>
#include <cstdint>
#include <optional>
namespace misaki {
struct Mega60Result{
    std::array<bool,5> stages{};
    std::uint32_t elfDependencies=0,relaRecords=0,services=0,serviceFailures=0;
    std::uint32_t gpuPixels=0,gpuCopies=0,guestProcesses=0,guestExits=0;
    std::uint32_t guestInstructions=0,contextSwitches=0;
    std::uint32_t encryptedSelfSegments=0,unmetFirmwarePrerequisites=0;
    bool importedAnalysisRefusedExecution=false,bootAttempted=false,homeMenuBooted=false;
};
std::optional<Mega60Result> runMega60Diagnostic();
}
