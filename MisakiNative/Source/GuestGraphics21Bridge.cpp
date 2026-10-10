#include "../Include/MisakiGPU21Bridge.h"
#include "../Include/GuestGraphics21.hpp"

#include <algorithm>

extern "C" int32_t misaki_gpu21_render(uint32_t frame_index, uint32_t effect_mode,
                                         uint8_t *rgba, size_t capacity,
                                         MisakiGPU21Report *report) {
    if (!report) return -1;
    *report = MisakiGPU21Report{};
    if (!rgba) return -1;
    try {
        const auto result = misaki::runG21GuestFrame(frame_index, effect_mode);
        if (!result) return -2;
        const auto &frame = result->frame;
        const auto &surface = frame.surface;
        if (capacity < surface.pixels.size()) return -3;
        std::copy(surface.pixels.begin(), surface.pixels.end(), rgba);
        report->abi_version = 1;
        report->width = misaki::g21Width;
        report->height = misaki::g21Height;
        report->bytes_written = static_cast<uint32_t>(surface.pixels.size());
        report->frame_index = frame_index;
        report->command_count = static_cast<uint32_t>(frame.packets.size());
        report->rectangles = surface.rectangles;
        report->sprites = surface.sprites;
        report->distinct_textures = surface.distinctTextures;
        report->state_changes = surface.stateChanges;
        report->scissor_changes = surface.scissorChanges;
        report->effect_mode = surface.effectMode;
        report->guest_instructions = result->guest.state.instructions;
        report->guest_service_calls = result->serviceCalls;
        report->guest_halted = result->guest.stop == misaki::X64Stop::halted ? 1u : 0u;
        report->stack_restored = result->guest.stackRestored() ? 1u : 0u;
        report->queue_drained = result->queueDrained ? 1u : 0u;
        report->first_sprite_x = result->firstSpriteX;
        report->second_sprite_x = result->secondSpriteX;
        report->submitted_fence = frame.fence;
        report->completed_fence = result->completedFence;
        report->checksum = surface.checksum;
        return 0;
    } catch (...) {
        return -2;
    }
}
