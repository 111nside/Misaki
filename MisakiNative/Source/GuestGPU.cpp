#include "../Include/GuestGPU.hpp"

#include <limits>
#include <utility>

namespace misaki {
namespace {
constexpr std::size_t maximumCommands = 64;
constexpr std::uint32_t maximumDimension = 2048;
constexpr std::uint64_t fnvOffset = 14695981039346656037ULL;
constexpr std::uint64_t fnvPrime = 1099511628211ULL;

bool validDimensions(std::uint32_t width, std::uint32_t height) {
    return width > 0 && height > 0 && width <= maximumDimension && height <= maximumDimension;
}
bool opaque(std::uint32_t rgba) { return (rgba & 0xFFu) == 0xFFu; }
bool rectangleFits(const GPUCommand &command,
                   std::uint32_t width, std::uint32_t height) {
    return command.width > 0 && command.height > 0 &&
           command.x < width && command.y < height &&
           command.width <= width - command.x &&
           command.height <= height - command.y;
}
void digest32(std::uint64_t &hash, std::uint32_t word) {
    // Canonical little-endian byte order independent of the host architecture.
    for (unsigned i = 0; i < 4; ++i) {
        hash ^= (word >> (8 * i)) & 0xFFu;
        hash *= fnvPrime;
    }
}
} // namespace

std::optional<std::uint64_t> validateGPUFrame(const GPUFrame &frame) {
    if (!validDimensions(frame.width, frame.height) || frame.commands.size() < 2 ||
        frame.commands.size() > maximumCommands) return std::nullopt;
    bool seenClear = false;
    bool seenPresent = false;
    std::uint64_t checksum = fnvOffset;
    digest32(checksum, frame.width);
    digest32(checksum, frame.height);
    digest32(checksum, static_cast<std::uint32_t>(frame.commands.size()));
    for (std::size_t i = 0; i < frame.commands.size(); ++i) {
        const GPUCommand &c = frame.commands[i];
        switch (c.opcode) {
        case GPUOpcode::clear:
            if (i != 0 || seenClear || !opaque(c.rgba) ||
                c.x != 0 || c.y != 0 || c.width != 0 || c.height != 0)
                return std::nullopt;
            seenClear = true;
            break;
        case GPUOpcode::rectangle:
            if (!seenClear || seenPresent || !opaque(c.rgba) ||
                !rectangleFits(c, frame.width, frame.height)) return std::nullopt;
            break;
        case GPUOpcode::present:
            if (!seenClear || seenPresent || i != frame.commands.size() - 1 ||
                c.x != 0 || c.y != 0 || c.width != 0 || c.height != 0 || c.rgba != 0)
                return std::nullopt;
            seenPresent = true;
            break;
        default: return std::nullopt;
        }
        digest32(checksum, static_cast<std::uint32_t>(c.opcode));
        digest32(checksum, c.x);
        digest32(checksum, c.y);
        digest32(checksum, c.width);
        digest32(checksum, c.height);
        digest32(checksum, c.rgba);
    }
    if (!seenPresent) return std::nullopt;
    return checksum;
}

GuestGPUBuilder::GuestGPUBuilder(std::uint32_t width, std::uint32_t height)
    : width_(width), height_(height) {}

bool GuestGPUBuilder::clear(std::uint32_t rgba) {
    if (!validDimensions(width_, height_) || hasClear_ || hasPresent_ || !opaque(rgba)) return false;
    commands_.push_back({GPUOpcode::clear, 0, 0, 0, 0, rgba});
    hasClear_ = true;
    return true;
}

bool GuestGPUBuilder::rectangle(std::uint32_t x, std::uint32_t y,
                                 std::uint32_t width, std::uint32_t height,
                                 std::uint32_t rgba) {
    GPUCommand command{GPUOpcode::rectangle, x, y, width, height, rgba};
    if (!hasClear_ || hasPresent_ || commands_.size() >= maximumCommands - 1 ||
        !opaque(rgba) || !rectangleFits(command, width_, height_)) return false;
    commands_.push_back(command);
    return true;
}

bool GuestGPUBuilder::present() {
    if (!hasClear_ || hasPresent_ || commands_.size() >= maximumCommands) return false;
    commands_.push_back({GPUOpcode::present, 0, 0, 0, 0, 0});
    hasPresent_ = true;
    return true;
}

std::optional<GPUFrame> GuestGPUBuilder::finish() const {
    if (!hasPresent_) return std::nullopt;
    GPUFrame frame{width_, height_, commands_, 0};
    const auto result = validateGPUFrame(frame);
    if (!result) return std::nullopt;
    frame.checksum = *result;
    return frame;
}

GPUFrame makeDemoGPUFrame() {
    GuestGPUBuilder gpu(320, 180);
    if (!gpu.clear(0x0C1426FFu) ||
        !gpu.rectangle(24, 24, 272, 132, 0x263A61FFu) ||
        !gpu.rectangle(36, 40, 100, 100, 0xB45AE4FFu) ||
        !gpu.rectangle(149, 52, 120, 32, 0x57D3E7FFu) ||
        !gpu.rectangle(149, 96, 90, 32, 0x6497F3FFu) ||
        !gpu.rectangle(0, 168, 320, 12, 0x172B4EFFu) ||
        !gpu.present()) return {};
    return gpu.finish().value_or(GPUFrame{});
}
} // namespace misaki
