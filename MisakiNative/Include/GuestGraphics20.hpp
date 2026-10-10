#pragma once

#include "GuestGraphics19.hpp"

#include <cstddef>
#include <cstdint>
#include <deque>
#include <optional>

namespace misaki {

// Misaki's own bounded guest-memory GPU command queue (not GCN/GNM/GNMX).
// Each guest command is four little-endian u64 words (32 bytes):
// opcode, packed XY, packed WH, packed RGBA/textureID.
constexpr std::uint64_t g20GPUService = 0x340;
constexpr std::uint64_t g20GuestQueueAddress = 0x5000;
constexpr std::size_t g20RecordBytes = 32;

struct G20Submission {
    std::uint64_t fence = 0;
    G19Scene scene;
    G19Surface surface;
};

// The queue owns COPIES of command data, never a pointer into guest memory.
// Submission is transactional: malformed packets never alter queue/fence state.
class GuestGraphicsQueue {
public:
    bool submit(const GuestMemory &memory, std::uint64_t address,
                std::uint32_t count, std::uint64_t fence);
    std::optional<G20Submission> take();
    std::size_t pending() const { return queue_.size(); }
    std::uint64_t lastSubmittedFence() const { return lastSubmittedFence_; }
    std::uint64_t completedFence() const { return completedFence_; }
private:
    std::deque<G20Submission> queue_;
    std::uint64_t lastSubmittedFence_ = 0;
    std::uint64_t completedFence_ = 0;
};

// Loads a self-authored x86-64 program, lets it patch a queue in its own RW
// guest memory, and routes SYSCALL 0x340 through the simulated service ABI.
struct G20Diagnostic {
    G20Submission submitted;
    X64ExecutionResult execution;
    std::uint32_t serviceCalls = 0;
    std::uint32_t commandCount = 0;
    std::uint32_t spriteX = 0;
    std::uint32_t spriteY = 0;
    bool queueDrained = false;
    std::uint64_t completedFence = 0;
};

std::optional<G20Diagnostic> runG20GuestFrame(std::uint32_t frameIndex);

} // namespace misaki
