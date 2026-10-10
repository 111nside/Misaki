#pragma once

#include <cstddef>
#include <cstdint>
#include <optional>
#include <vector>

namespace misaki {

// An original, deliberately small 2D guest drawing protocol; NOT AMD GCN,
// Sony GNM/GNMX, shaders from PS4 binaries, or a PS4 GPU command processor.
enum class GPUOpcode : std::uint32_t { clear = 1, rectangle = 2, present = 3 };

struct GPUCommand {
    GPUOpcode opcode = GPUOpcode::clear;
    std::uint32_t x = 0;
    std::uint32_t y = 0;
    std::uint32_t width = 0;
    std::uint32_t height = 0;
    std::uint32_t rgba = 0;
};

struct GPUFrame {
    std::uint32_t width = 0;
    std::uint32_t height = 0;
    std::vector<GPUCommand> commands;
    std::uint64_t checksum = 0;
};

class GuestGPUBuilder {
public:
    GuestGPUBuilder(std::uint32_t width, std::uint32_t height);
    bool clear(std::uint32_t rgba);
    bool rectangle(std::uint32_t x, std::uint32_t y,
                   std::uint32_t width, std::uint32_t height, std::uint32_t rgba);
    bool present();
    std::optional<GPUFrame> finish() const;

private:
    std::uint32_t width_;
    std::uint32_t height_;
    std::vector<GPUCommand> commands_;
    bool hasClear_ = false;
    bool hasPresent_ = false;
};

// Validates an entire frame, including opcode sequencing and a bounded budget.
// The checksum is deterministic across platforms and includes every u32 field.
std::optional<std::uint64_t> validateGPUFrame(const GPUFrame &frame);
GPUFrame makeDemoGPUFrame();

} // namespace misaki
