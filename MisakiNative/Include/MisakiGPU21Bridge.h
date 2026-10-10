#ifndef MISAKI_GPU21_BRIDGE_H
#define MISAKI_GPU21_BRIDGE_H

#include <stddef.h>
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif

// Original research-only GPU queue. Not a PS4 graphics interface.
typedef struct MisakiGPU21Report {
    uint32_t abi_version;
    uint32_t width;
    uint32_t height;
    uint32_t bytes_written;
    uint32_t frame_index;
    uint32_t command_count;
    uint32_t rectangles;
    uint32_t sprites;
    uint32_t distinct_textures;
    uint32_t state_changes;
    uint32_t scissor_changes;
    uint32_t effect_mode;
    uint32_t guest_instructions;
    uint32_t guest_service_calls;
    uint32_t guest_halted;
    uint32_t stack_restored;
    uint32_t queue_drained;
    uint32_t first_sprite_x;
    uint32_t second_sprite_x;
    uint64_t submitted_fence;
    uint64_t completed_fence;
    uint64_t checksum;
} MisakiGPU21Report;

// Copies an entire 320x180 RGBA8 frame on success only. Negative on failure:
// -1 null arguments, -2 guest/validation failure, -3 capacity too small.
// Effect modes: 0 normal, 1 grayscale, 2 invert, 3 scanlines.
int32_t misaki_gpu21_render(uint32_t frame_index, uint32_t effect_mode,
                             uint8_t *rgba, size_t capacity,
                             MisakiGPU21Report *report);

#ifdef __cplusplus
}
#endif
#endif
