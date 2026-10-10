#pragma once

#include "GuestGraphics20.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <deque>
#include <optional>
#include <vector>

namespace misaki {

// Original Misaki rendering experiment: NOT AMD GCN, GNM, or GNMX.
// 32-byte packets in guest RW memory: kind:u64, XY:u64, WH:u64, data:u64.
// Rectangles use packed RGBA8 in the low 32 data bits. Sprites use a
// texture ID there. State: XY = blendMode/effectMode, data=tintRGBA.
// Scissor: XY=origin, WH=size; scissor persists until changed.
enum class G21Opcode : std::uint32_t {
    clear = 1, fill = 2, sprite = 3, present = 4, state = 5, scissor = 6
};

enum class G21Blend : std::uint32_t { alpha = 0, additive = 1 };
enum class G21Effect : std::uint32_t { normal = 0, grayscale = 1, invert = 2, scanlines = 3 };

constexpr std::uint64_t g21GPUService = 0x350;
constexpr std::uint64_t g21QueueAddress = 0x6000;
constexpr std::size_t g21RecordBytes = 32;
constexpr std::uint32_t g21Width = 320;
constexpr std::uint32_t g21Height = 180;
constexpr std::uint32_t g21MaximumCommands = 32;

struct G21Packet {
    G21Opcode opcode = G21Opcode::clear;
    std::uint32_t x = 0, y = 0, width = 0, height = 0;
    std::uint32_t data = 0, extra = 0;
};

struct G21Surface {
    std::vector<std::uint8_t> pixels; // 320x180 RGBA8
    std::uint64_t checksum = 0;
    std::uint32_t rectangles = 0;
    std::uint32_t sprites = 0;
    std::uint32_t stateChanges = 0;
    std::uint32_t scissorChanges = 0;
    std::uint32_t distinctTextures = 0;
    std::uint32_t effectMode = 0;
};

struct G21Submission {
    std::uint64_t fence = 0;
    G21Surface surface;
    std::vector<G21Packet> packets;
};

// Validate the *entire* packet list before drawing. A returned pixel surface
// is owned by the host; the GPU never retains a pointer to guest memory.
std::optional<G21Surface> rasterizeG21(const std::vector<G21Packet> &packets);

class GuestGraphics21Queue {
public:
    bool submit(const GuestMemory &memory, std::uint64_t guestAddress,
                std::uint32_t count, std::uint64_t fence);
    std::optional<G21Submission> take();
    std::size_t pending() const { return pending_.size(); }
    std::uint64_t lastSubmittedFence() const { return lastSubmittedFence_; }
    std::uint64_t completedFence() const { return completedFence_; }
private:
    std::deque<G21Submission> pending_;
    std::uint64_t lastSubmittedFence_ = 0;
    std::uint64_t completedFence_ = 0;
};

struct G21Diagnostic {
    G21Submission frame;
    X64ExecutionResult guest;
    std::uint32_t serviceCalls = 0;
    std::uint32_t firstSpriteX = 0;
    std::uint32_t secondSpriteX = 0;
    bool queueDrained = false;
    std::uint64_t completedFence = 0;
};

// Guest x86-64 rewrites TWO texture positions and a shader-effect register in
// its virtual queue, then submits the queue with a synthetic SYSCALL service.
std::optional<G21Diagnostic> runG21GuestFrame(std::uint32_t frameIndex,
                                                std::uint32_t requestedEffect);

} // namespace misaki
