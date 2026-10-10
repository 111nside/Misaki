#ifndef MISAKI_GPU_BRIDGE_H
#define MISAKI_GPU_BRIDGE_H

#include <stddef.h>
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif

// Stable, host-neutral drawing commands. RGB bytes occupy bits 31..8;
// alpha is the bottom byte. This is NOT the PlayStation 4 command format.
// kinds: 1 clear, 2 filled rectangle, 3 present.
typedef struct MisakiGPUCommand {
    uint32_t kind;
    uint32_t x;
    uint32_t y;
    uint32_t width;
    uint32_t height;
    uint32_t rgba;
} MisakiGPUCommand;

typedef struct MisakiGPUFrameInfo {
    uint32_t abi_version;
    uint32_t width;
    uint32_t height;
    uint32_t command_count;
    uint32_t rectangle_count;
    uint32_t clear_count;
    uint32_t present_count;
    uint64_t checksum;
} MisakiGPUFrameInfo;

// 0 success, -1 invalid arguments, -2 invalid scene, -3 too-small output.
// Never writes beyond capacity or writes a partial command stream on error.
int32_t misaki_gpu_demo_frame(MisakiGPUCommand *output, size_t capacity,
                              MisakiGPUFrameInfo *info);

#ifdef __cplusplus
}
#endif
#endif
