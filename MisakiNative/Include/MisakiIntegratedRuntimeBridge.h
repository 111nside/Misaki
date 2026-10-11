#ifndef MISAKI_INTEGRATED_RUNTIME_BRIDGE_H
#define MISAKI_INTEGRATED_RUNTIME_BRIDGE_H
#include <stddef.h>
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif

typedef struct MisakiIntegratedRuntimeReport {
    uint32_t abi_version;
    uint32_t passed_31;
    uint32_t passed_32;
    uint32_t passed_33;
    uint32_t passed_34;
    uint32_t passed_35;
    uint32_t process_count;
    uint32_t instructions;
    uint32_t context_switches;
    uint32_t vfs_opens;
    uint32_t vfs_reads;
    uint32_t vfs_closes;
    uint32_t gpu_calls;
    uint32_t pm4_packets;
    uint32_t shader_operations;
    uint32_t loaded_modules;
    uint32_t resolved_imports;
    uint32_t framebuffer_sprites;
    uint32_t framebuffer_width;
    uint32_t framebuffer_height;
    uint64_t framebuffer_checksum;
    uint64_t submitted_fence;
    uint64_t completed_fence;
    uint64_t elf_rax;
    uint64_t guest_pid;
    uint32_t output_record_7;
    uint32_t bytes_written;
} MisakiIntegratedRuntimeReport;

// Full frame is 320 * 180 * 4 RGBA8 bytes. -1 bad pointers,
// -2 native integration failure, -3 insufficient output space.
// On error, the output pixel buffer is not modified.
int32_t misaki_integrated_runtime_demo(uint8_t *pixels, size_t capacity,
                                        MisakiIntegratedRuntimeReport *report);
#ifdef __cplusplus
}
#endif
#endif
