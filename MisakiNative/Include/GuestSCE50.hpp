#pragma once
#include "GuestSCE46.hpp"
#include "GuestSCE47.hpp"
#include "GuestVM48.hpp"
#include "GuestGCN49.hpp"
#include "GuestSCEKernel.hpp"
#include "GuestBootReadiness.hpp"
#include <array>
#include <cstdint>
#include <optional>
namespace misaki {
// A single synthetic shared GuestMemory session. Stage 50 PASS means all five
// fixture diagnostics worked and genuine PS4 boot remains explicitly BLOCKED.
struct SCE50Report {
    std::array<bool,5> stages{};
    std::uint32_t segments=0,relocations=0,modules=0,imports=0;
    std::uint64_t relocationValue=0,guestRax=0;
    std::uint32_t mappedGuestPages=0,serviceCalls=0,serviceFailures=0;
    std::uint32_t pm4Packets=0,sprites=0;
    std::uint64_t framebufferChecksum=0,completedFence=0;
    std::uint32_t firmwareBlockers=0;
    bool firmwareAttempted=false,homeMenuBooted=false;
};
std::optional<SCE50Report> runSCE50Diagnostic();
}
