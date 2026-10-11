#pragma once

#include "GuestDynamicExecution.hpp"
#include "GuestFusedGCN30.hpp"
#include "GuestGraphics21.hpp"
#include "GuestModuleLifecycle.hpp"
#include "GuestPM4Indirect.hpp"
#include "GuestProcesses.hpp"

#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <vector>

namespace misaki {

// Milestones 31-35: one bounded, cooperative process runtime. All service IDs
// below are original Misaki diagnostic ABI numbers, NEVER PS4/SCE syscalls.
namespace runtime_service {
constexpr std::uint64_t gpuSubmit = 0x510;   // RDI=commands, RSI=count, RDX=fence
constexpr std::uint64_t gpuPoll = 0x511;     // RAX=last completed fence
constexpr std::uint64_t gpuWait = 0x512;     // RDI=desired fence (drains queue)
constexpr std::uint64_t shaderRun = 0x513;   // RDI=GCN code, RSI=words, RDX=inputDesc, RCX=outputDesc
constexpr std::uint64_t pm4Inspect = 0x514;  // RDI=root PM4, RSI=DWORD count
constexpr std::uint64_t failure = UINT64_MAX;
}

class IntegratedProcessServices final : public IGuestServiceDispatcher {
public:
    IntegratedProcessServices(const GuestVirtualFileSystem &filesystem, std::uint32_t pid);
    GuestServiceAction dispatch(GuestMemory &memory, X64State &state,
                                std::uint32_t threadID) override;
    std::uint32_t pid() const { return pid_; }
    const GuestProcessServices &files() const { return files_; }
    std::uint32_t gpuCalls() const { return gpuCalls_; }
    std::uint32_t shaderCalls() const { return shaderCalls_; }
    std::uint32_t pm4Calls() const { return pm4Calls_; }
    std::uint32_t rejectedCalls() const { return rejectedCalls_; }
    std::uint64_t submittedFence() const { return gpu_.lastSubmittedFence(); }
    std::uint64_t completedFence() const { return gpu_.completedFence(); }
    std::size_t pendingGPU() const { return gpu_.pending(); }
    const G21Submission *lastFrame() const { return lastFrame_ ? &*lastFrame_ : nullptr; }
    const FusedGCN30Reference *lastShader() const { return lastShader_ ? &*lastShader_ : nullptr; }
    const PM4IndirectTrace *lastPM4() const { return lastPM4_ ? &*lastPM4_ : nullptr; }
    const std::vector<std::uint8_t> &lastFileRead() const { return lastFileRead_; }
private:
    std::uint32_t pid_;
    GuestProcessServices files_;
    GuestGraphics21Queue gpu_;
    std::optional<G21Submission> lastFrame_;
    std::optional<FusedGCN30Reference> lastShader_;
    std::optional<PM4IndirectTrace> lastPM4_;
    std::vector<std::uint8_t> lastFileRead_;
    std::uint32_t gpuCalls_ = 0, shaderCalls_ = 0, pm4Calls_ = 0, rejectedCalls_ = 0;
};

struct IntegratedProcessResult {
    std::uint32_t pid = 0;
    X64ExecutionResult execution;
    std::uint32_t opens = 0, reads = 0, closes = 0;
    std::uint32_t gpuCalls = 0, shaderCalls = 0, pm4Calls = 0, rejectedCalls = 0;
    std::uint64_t submittedFence = 0, completedFence = 0;
    std::uint32_t pendingGPU = 0;
    std::uint32_t loadedModules = 0, resolvedImports = 0;
    std::uint64_t framebufferChecksum = 0;
    std::uint32_t framebufferSprites = 0, pm4Packets = 0;
    std::uint32_t shaderExecuted = 0, shaderLoaded = 0, shaderStored = 0;
    std::vector<std::uint8_t> lastRead;
    std::vector<std::uint8_t> framebuffer; // most recent CPU-rendered RGBA8 surface
};
struct IntegratedRuntimeResult {
    bool completed = false;
    std::uint32_t instructions = 0;
    std::uint32_t contextSwitches = 0;
    std::vector<IntegratedProcessResult> processes;
};

class IntegratedGuestRuntime final {
public:
    IntegratedGuestRuntime();
    ~IntegratedGuestRuntime();
    IntegratedGuestRuntime(const IntegratedGuestRuntime &) = delete;
    IntegratedGuestRuntime &operator=(const IntegratedGuestRuntime &) = delete;
    bool addFile(const std::string &path, std::vector<std::uint8_t> contents);
    bool addProcess(GuestMemory memory, std::uint64_t entry);
    // Ordinary, independently authored ET_DYN ELF and RX guest modules only.
    // Transactional: a rejected load never creates a process.
    bool addDynamicProcess(const std::vector<std::uint8_t> &elf,
                           const std::vector<GuestModuleDefinition> &definitions,
                           const std::string &root, std::uint64_t loadBias);
    IntegratedRuntimeResult run(std::uint32_t quantum = 4,
                                std::uint32_t maxInstructions = 1000);
    std::size_t processCount() const { return slots_.size(); }
    const IntegratedProcessServices *services(std::size_t index) const;
private:
    struct Slot;
    GuestVirtualFileSystem fs_;
    std::vector<std::unique_ptr<Slot>> slots_;
    bool ran_ = false;
    bool addPrepared(GuestMemory memory, std::uint64_t entry,
                     std::unique_ptr<GuestModuleLifecycle> modules,
                     std::uint32_t imports);
};

// Independently authored fixture helpers for the integrated runtime tests.
std::vector<std::uint8_t> makeRuntime31Program(std::uint32_t tag);
std::vector<std::uint8_t> makeRuntime35Program();
std::vector<std::uint8_t> makeRuntimeG21Commands();
bool installRuntimeGCN(GuestMemory &memory);
bool installRuntimePM4(GuestMemory &memory);
bool installRuntime35Memory(GuestMemory &memory);
std::vector<GuestModuleDefinition> makeRuntime34Modules();

struct IntegratedMegaDiagnostic {
    IntegratedRuntimeResult runtime;
    bool stage31 = false, stage32 = false, stage33 = false,
         stage34 = false, stage35 = false;
};
std::optional<IntegratedMegaDiagnostic> runIntegratedMegaDiagnostic();
} // namespace misaki
