#pragma once
#include "GuestX64Backend.hpp"
#include <cstdint>
#include <string>
#include <vector>
namespace misaki {
// MISAKI'S PRIVATE TEST ABI. These IDs and semantics are not PS4 syscalls.
namespace sce_shim {
constexpr std::uint64_t pid=0x741;
constexpr std::uint64_t clock=0x742;
constexpr std::uint64_t write=0x743;
constexpr std::uint64_t exit=0x744;
constexpr std::uint64_t failure=UINT64_MAX;
}
class SCEKernelShim final : public IGuestServiceDispatcher {
public:
    explicit SCEKernelShim(std::uint32_t pid, std::uint64_t startTicks=1000);
    GuestServiceAction dispatch(GuestMemory &memory, X64State &cpu,
                                std::uint32_t threadID) override;
    std::uint32_t calls() const { return calls_; }
    std::uint32_t failures() const { return failures_; }
    std::uint64_t lastTicks() const { return lastTicks_; }
    std::uint64_t lastPID() const { return lastPID_; }
    std::uint64_t exitCode() const { return exitCode_; }
    bool exited() const { return exited_; }
    const std::vector<std::uint8_t> &output() const { return output_; }
private:
    std::uint32_t pid_;
    std::uint64_t nextTick_;
    std::uint64_t lastTicks_=0,lastPID_=0,exitCode_=0;
    std::uint32_t calls_=0,failures_=0;
    bool exited_=false;
    std::vector<std::uint8_t> output_;
};
std::vector<std::uint8_t> makeSCEKernelGuestFixture();
} // namespace misaki
