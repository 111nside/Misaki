#include "GuestGraphics19.hpp"
#include "MisakiGPU19Bridge.h"

#include <algorithm>
#include <cstdlib>
#include <cstdint>
#include <iostream>
#include <limits>
#include <vector>

namespace {
unsigned checks = 0;
void check(bool condition, const char *why) {
    ++checks;
    if (!condition) { std::cerr << "FAIL: " << why << '\n'; std::exit(EXIT_FAILURE); }
}
}
int main() {
    using namespace misaki;
    const auto frame0 = runG19GuestFrame(0);
    check(bool(frame0), "CPU to GPU diagnostic frame 0");
    check(frame0->spriteX == 32 && frame0->spriteY == 78, "guest-provided coordinates");
    check(frame0->execution.state.instructions == 6, "six guest instructions");
    check(frame0->execution.stop == X64Stop::halted, "guest halted");
    check(frame0->execution.rax() == 1, "guest syscall acknowledgement");
    check(frame0->execution.stackRestored(), "guest stack restored");
    check(frame0->serviceCalls == 1, "one GPU guest service call");
    check(frame0->surface.width == 320 && frame0->surface.height == 180, "framebuffer dimensions");
    check(frame0->surface.pixels.size() == 320 * 180 * 4, "framebuffer byte size");
    check(frame0->surface.filledRects == 4 && frame0->surface.texturedSprites == 1,
          "texture and rectangles composed");
    check(frame0->commandCount == 7, "valid command sequence");
    check(frame0->surface.checksum != 0, "pixel checksum present");
    check(frame0->surface.checksum == runG19GuestFrame(0)->surface.checksum,
          "deterministic framebuffer");
    check(runG19GuestFrame(1)->spriteX == 34, "animated guest write moves sprite");
    check(runG19GuestFrame(89)->spriteX == 210, "last frame within canvas");
    check(runG19GuestFrame(90)->spriteX == 32, "animation wraps at 90");
    check(runG19GuestFrame(90)->surface.checksum == frame0->surface.checksum,
          "frame cycle is deterministic");
    check(runG19GuestFrame(1)->surface.checksum != frame0->surface.checksum,
          "different guest position creates different pixels");

    const auto &p = frame0->surface.pixels;
    const auto sample = [&](std::uint32_t x, std::uint32_t y, unsigned c) {
        return p[(std::size_t(y) * 320 + x) * 4 + c];
    };
    check(sample(0, 0, 0) == 12 && sample(0, 0, 1) == 20 &&
          sample(0, 0, 2) == 38 && sample(0, 0, 3) == 255,
          "clear to opaque navy RGBA");
    check(sample(20, 20, 0) == 38 && sample(20, 20, 1) == 58,
          "solid rectangle overwrites background");
    check(sample(50, 100, 0) != sample(35, 100, 0), "sprite checker colors appear");
    check(sample(32, 78, 0) == 38, "transparent sprite corner keeps background");

    G19Scene scene;
    scene.width = 16; scene.height = 16;
    scene.spriteTexture.width = scene.spriteTexture.height = 1;
    scene.spriteTexture.rgba = {255, 0, 0, 128};
    scene.commands = {
        {G19Opcode::clear, 0, 0, 0, 0, 0x0000FFFF, 0},
        {G19Opcode::sprite, 2, 2, 4, 4, 0, 1},
        {G19Opcode::present, 0, 0, 0, 0, 0, 0}
    };
    auto alpha = rasterizeG19(scene);
    check(bool(alpha), "alpha sprite accepted");
    auto pixel = [&](const G19Surface &surface, unsigned x, unsigned y, unsigned c) {
        return surface.pixels[(std::size_t(y) * surface.width + x) * 4 + c];
    };
    check(pixel(*alpha, 2, 2, 0) == 128 && pixel(*alpha, 2, 2, 1) == 0 &&
          pixel(*alpha, 2, 2, 2) == 127 && pixel(*alpha, 2, 2, 3) == 255,
          "correct alpha blend red-over-blue");
    check(pixel(*alpha, 1, 1, 2) == 255, "framebuffer outside sprite intact");
    scene.commands[1].x = 15;
    check(!rasterizeG19(scene), "reject sprite beyond surface bounds");
    scene.commands[1].x = 2;
    scene.commands[1].textureID = 2;
    check(!rasterizeG19(scene), "reject unknown texture ID");
    scene.commands[1].textureID = 1;
    scene.spriteTexture.rgba.pop_back();
    check(!rasterizeG19(scene), "reject truncated texture upload");
    scene.spriteTexture.rgba.push_back(128);
    scene.commands[0].rgba = 0xFF000080;
    check(!rasterizeG19(scene), "reject nonopaque clear");
    scene.commands[0].rgba = 0x0000FFFF;
    scene.commands[0].opcode = G19Opcode::present;
    check(!rasterizeG19(scene), "reject present before clear");
    scene.commands[0].opcode = G19Opcode::clear;
    scene.commands[2].opcode = G19Opcode::sprite;
    check(!rasterizeG19(scene), "reject missing final present");
    scene.commands[2].opcode = G19Opcode::present;
    scene.width = 2000;
    check(!rasterizeG19(scene), "reject canvas over max dimension");
    scene.width = 16;
    scene.spriteTexture.width = 65;
    check(!rasterizeG19(scene), "reject oversize texture");
    scene.spriteTexture.width = 1;
    scene.commands[1].width = std::numeric_limits<std::uint32_t>::max();
    check(!rasterizeG19(scene), "reject overflow-sized draw");
    scene.commands[1].width = 4;
    scene.commands[1].opcode = static_cast<G19Opcode>(999);
    check(!rasterizeG19(scene), "reject unrecognized command");

    MisakiGPU19Report report{};
    std::vector<std::uint8_t> dest(320 * 180 * 4, 0xA5);
    check(misaki_gpu19_render(0, dest.data(), dest.size(), &report) == 0,
          "C bridge outputs full framebuffer");
    check(report.abi_version == 1 && report.frame_index == 0, "bridge ABI and frame");
    check(report.width == 320 && report.height == 180 &&
          report.bytes_written == dest.size(), "bridge framebuffer shape");
    check(report.command_count == 7 && report.rectangles == 4 && report.sprites == 1,
          "bridge draw command counts");
    check(report.guest_instructions == 6 && report.guest_service_calls == 1,
          "bridge guest execution verified");
    check(report.guest_halted && report.stack_restored && report.sprite_x == 32 &&
          report.sprite_y == 78, "bridge guest stop state");
    check(report.checksum == frame0->surface.checksum && dest == frame0->surface.pixels,
          "bridge bytes match C++ rasterizer");
    dest[0] = 0x5A;
    check(misaki_gpu19_render(0, dest.data(), dest.size() - 1, &report) == -3,
          "reject short buffer");
    check(dest[0] == 0x5A && report.bytes_written == 0 && report.checksum == 0,
          "short buffer leaves all pixels and report untouched / cleared");
    check(misaki_gpu19_render(0, nullptr, dest.size(), &report) == -1,
          "reject null framebuffer pointer");
    check(misaki_gpu19_render(0, dest.data(), dest.size(), nullptr) == -1,
          "reject null report");
    check(misaki_gpu19_render(std::numeric_limits<std::uint32_t>::max(),
          dest.data(), dest.size(), &report) == 0, "bounded frame index arithmetic");
    std::cout << "PASS: " << checks << " native graphics19 assertions\n";
}
