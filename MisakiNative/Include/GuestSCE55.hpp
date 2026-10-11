#pragma once
#include "GuestSCE51.hpp"
#include "GuestSCE52.hpp"
#include "GuestKernel53.hpp"
#include "GuestGCN54.hpp"
#include "GuestSCE50.hpp"
#include <array>
#include <cstdint>
#include <optional>
namespace misaki {
// End-to-end compatibility READINESS report for self-authored inputs.
// A stage pass never authorizes loading or booting authentic PS4 firmware.
struct SCE55Report {
    std::array<bool,5> stages{};
    std::uint32_t elfSegments=0,elfRelocations=0;
    std::uint32_t selfEncryptedSegments=0,sceDynamicTags=0;
    std::uint32_t syntheticNids=0,processes=0,mappedPages=0;
    std::uint32_t pm4Packets=0,gcnLanes=0,gcnInstructions=0;
    std::uint32_t outputIndex7=0,firmwareBlockers=0;
    std::uint64_t guestRax=0,shaderHash=0;
    bool encryptedSelfRefused=false;
    bool realFirmwareAttempted=false,homeMenuBooted=false;
};
std::optional<SCE55Report> runSCE55Diagnostic();
} // namespace misaki
