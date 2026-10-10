#ifndef MISAKI_GPU20_BRIDGE_H
#define MISAKI_GPU20_BRIDGE_H
#include <stddef.h>
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif

typedef struct MisakiGPU20Report {
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
    uint32_t queue_drained;
    uint32_t sprite_x;
    uint32_t sprite_y;
    uint64_t submitted_fence;
    uint64_t completed_fence;
    uint64_t checksum;
} MisakiGPU20Report;

// The framebuffer is 320*180*4 bytes; the bridge copies only on success.
// Returns -1 for null parameters, -2 for invalid guest submission,
// -3 for insufficient capacity (without modifying pixel output).
int32_t misaki_gpu20_render(uint32_t frame_index, uint8_t *rgba,
                            size_t capacity, MisakiGPU20Report *report);
#ifdef __cplusplus
}
#endif
#endif
