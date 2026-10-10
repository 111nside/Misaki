#ifndef MISAKI_GPU19_BRIDGE_H
#define MISAKI_GPU19_BRIDGE_H
#include <stddef.h>
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif

typedef struct MisakiGPU19Report {
    uint32_t abi_version;
    uint32_t width;
    uint32_t height;
    uint32_t bytes_written;
    uint32_t frame_index;
    uint32_t command_count;
    uint32_t rectangles;
    uint32_t sprites;
    uint32_t guest_instructions;
    uint32_t guest_service_calls;
    uint32_t guest_halted;
    uint32_t stack_restored;
    uint32_t sprite_x;
    uint32_t sprite_y;
    uint64_t checksum;
} MisakiGPU19Report;

// Outputs an RGBA8 framebuffer; size must be >= width * height * 4.
// Returns 0 success; -1 invalid args, -2 guest/graphics error, -3 capacity.
// Errors NEVER write a partial framebuffer; report is zeroed when possible.
int32_t misaki_gpu19_render(uint32_t frame_index, uint8_t *rgba,
                            size_t capacity, MisakiGPU19Report *report);
#ifdef __cplusplus
}
#endif
#endif
