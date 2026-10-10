#include "../Include/MisakiGPU19Bridge.h"
#include "../Include/GuestGraphics19.hpp"

#include <algorithm>

extern "C" int32_t misaki_gpu19_render(uint32_t frame_index,
                                        uint8_t *rgba, size_t capacity,
                                        MisakiGPU19Report *report) {
    if (!report) return -1;
    *report = MisakiGPU19Report{};
    if (!rgba) return -1;
    try {
        auto result = misaki::runG19GuestFrame(frame_index);
        if (!result) return -2;
        if (capacity < result->surface.pixels.size()) return -3;
        std::copy(result->surface.pixels.begin(), result->surface.pixels.end(), rgba);
        report->abi_version = 1;
        report->width = result->surface.width;
        report->height = result->surface.height;
        report->bytes_written = static_cast<uint32_t>(result->surface.pixels.size());
        report->frame_index = frame_index;
        report->command_count = result->commandCount;
        report->rectangles = result->surface.filledRects;
        report->sprites = result->surface.texturedSprites;
        report->guest_instructions = result->execution.state.instructions;
        report->guest_service_calls = result->serviceCalls;
        report->guest_halted = result->execution.stop == misaki::X64Stop::halted ? 1u : 0u;
        report->stack_restored = result->execution.stackRestored() ? 1u : 0u;
        report->sprite_x = result->spriteX;
        report->sprite_y = result->spriteY;
        report->checksum = result->surface.checksum;
        return 0;
    } catch (...) {
        return -2;
    }
}
