#pragma once

#include "GuestX64Backend.hpp"

#include <cstdint>
#include <map>
#include <memory>
#include <optional>
#include <string>
#include <vector>

namespace misaki {

// Intentionally original, isolated test ABI. Numbers are NOT PS4/FreeBSD syscall IDs.
// Inputs: RAX = service ID, RDI = first argument, RSI = second argument.
namespace guest_service {
constexpr std::uint64_t pageSize = 0x100;
constexpr std::uint64_t write = 0x101;
constexpr std::uint64_t yield = 0x102;
constexpr std::uint64_t exit = 0x103;
constexpr std::uint64_t threadID = 0x104;
}

class GuestServiceEnvironment final : public IGuestServiceDispatcher {
public:
    GuestServiceAction dispatch(GuestMemory &memory, X64State &state,
                                std::uint32_t threadID) override;
    const std::string &output() const { return output_; }
    const std::map<std::uint32_t, std::uint64_t> &exitCodes() const { return exits_; }
    std::uint32_t serviceCalls() const { return calls_; }
    std::uint32_t writes() const { return writes_; }
    std::uint32_t yields() const { return yields_; }
private:
    std::string output_;
    std::map<std::uint32_t, std::uint64_t> exits_;
    std::uint32_t calls_ = 0;
    std::uint32_t writes_ = 0;
    std::uint32_t yields_ = 0;
};

struct GuestScheduleReport {
    bool completed = false;
    std::uint32_t instructions = 0;
    std::uint32_t yieldEvents = 0;
    std::vector<X64ExecutionResult> threadResults;
    std::vector<std::uint32_t> dispatchOrder;
};

// Cooperative round-robin simulation: independent virtual-memory contexts,
// deterministic quanta. This is NOT pthreads or true parallel guest execution.
class GuestRoundRobinScheduler {
public:
    bool addThread(GuestMemory memory, std::uint64_t entry);
    GuestScheduleReport run(std::uint32_t quantum = 4,
                            std::uint32_t maxTotalInstructions = 1000);
    const GuestServiceEnvironment &services() const { return services_; }
private:
    GuestServiceEnvironment services_;
    std::vector<std::unique_ptr<PortableX64Backend>> threads_;
    bool hasRun_ = false;
};

struct GuestServiceDiagnostic {
    GuestScheduleReport scheduler;
    std::string output;
    std::uint32_t calls = 0;
    std::uint32_t writes = 0;
    std::uint64_t pageSize = 4096;
};

// Two independently loaded, self-authored ELF guest programs exercise five
// native toy services and context switching on the expanded C++ CPU.
std::optional<GuestServiceDiagnostic> runGuestServiceDiagnostic();

} // namespace misaki
