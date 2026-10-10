#pragma once

#include "GuestX64Backend.hpp"

#include <cstddef>
#include <cstdint>
#include <optional>
#include <vector>

namespace misaki {

// Original, bounded 2D drawing format. Not Sony GNM/GNMX or AMD GCN.
enum class G19Opcode : std::uint32_t { clear = 1, fill = 2, sprite = 3, present = 4 };

struct G19Texture {
    std::uint32_t width = 0;
    std::uint32_t height = 0;
    std::vector<std::uint8_t> rgba; // top-left-origin RGBA8
};

struct G19Command {
    G19Opcode opcode = G19Opcode::clear;
    std::uint32_t x = 0, y = 0, width = 0, height = 0;
    std::uint32_t rgba = 0;
    std::uint32_t textureID = 0;
};

struct G19Scene {
    std::uint32_t width = 320, height = 180;
    G19Texture spriteTexture;
    std::vector<G19Command> commands;
};

struct G19Surface {
    std::uint32_t width = 0, height = 0;
    std::vector<std::uint8_t> pixels; // 4 bytes/pixel, owned storage
    std::uint64_t checksum = 0;
    std::uint32_t filledRects = 0, texturedSprites = 0;
};

// Validates an entire scene before drawing. No unbounded allocations, host
// pointers, arbitrary file IO, or partially updated output on failure.
std::optional<G19Surface> rasterizeG19(const G19Scene &scene);
G19Texture makeG19SpriteTexture();

// A synthetic x86-64 guest stores X into its own RW address space, then
// submits the guest address with SYSCALL 0x330. No host GPU handles leak into
// the guest, and the code executes through PortableX64Backend.
struct G19Diagnostic {
    G19Surface surface;
    X64ExecutionResult execution;
    std::uint32_t serviceCalls = 0, spriteX = 0, spriteY = 0;
    std::uint32_t commandCount = 0;
};
std::optional<G19Diagnostic> runG19GuestFrame(std::uint32_t frameIndex);

} // namespace misaki
