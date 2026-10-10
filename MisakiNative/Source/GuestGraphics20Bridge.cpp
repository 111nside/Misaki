#include "../Include/MisakiGPU20Bridge.h"
#include "../Include/GuestGraphics20.hpp"

#include <algorithm>

extern "C" int32_t misaki_gpu20_render(uint32_t frame_index, uint8_t *rgba,
                                         size_t capacity, MisakiGPU20Report *report) {
    if (!report) return -1;
    *report = MisakiGPU20Report{};
    if (!rgba) return -1;
    try {
        auto result = misaki::runG20GuestFrame(frame_index);
        if (!result) return -2;
        const auto &surface = result->submitted.surface;
        if (capacity < surface.pixels.size()) return -3;
        std::copy(surface.pixels.begin(), surface.pixels.end(), rgba);
        report->abi_version = 1;
        report->width = surface.width;
        report->height = surface.height;
        report->bytes_written = static_cast<uint32_t>(surface.pixels.size());
        report->frame_index = frame_index;
        report->command_count = result->commandCount;
        report->rectangles = surface.filledRects;
        report->sprites = surface.texturedSprites;
        report->guest_instructions = result->execution.state.instructions;
        report->guest_service_calls = result->serviceCalls;
        report->guest_halted = result->execution.stop == misaki::X64Stop::halted ? 1u : 0u;
        report->stack_restored = result->execution.stackRestored() ? 1u : 0u;
        report->queue_drained = result->queueDrained ? 1u : 0u;
        report->sprite_x = result->spriteX;
        report->sprite_y = result->spriteY;
        report->submitted_fence = result->submitted.fence;
        report->completed_fence = result->completedFence;
        report->checksum = surface.checksum;
        return 0;
    } catch (...) {
        return -2;
    }
}
