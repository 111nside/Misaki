#ifndef MISAKI_PM4_24_BRIDGE_H
#define MISAKI_PM4_24_BRIDGE_H
#include <stddef.h>
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif

// Read-only draw-time AMD PM4 register metadata. NOT Sony firmware/GPU execution.
// status per draw: 0 consistent metadata; 1 missing; 2 target disabled;
// 3 scissor invalid; 4 address invalid; 5 extra render target;
// 6 unsupported raw color format. Status 0 does NOT mean renderable.
typedef struct MisakiPM424Draw {
    uint64_t packet_address;
    uint64_t color_address;
    uint64_t pixel_shader_address;
    uint32_t packet_index;
    uint32_t depth;
    uint32_t vertex_count;
    uint32_t scissor_x;
    uint32_t scissor_y;
    uint32_t scissor_width;
    uint32_t scissor_height;
    uint32_t pitch_tile_max;
    uint32_t color_format_field;
    uint32_t target_mask;
    uint32_t observed_mask;
    uint32_t state_status;
} MisakiPM424Draw;

typedef struct MisakiPM424Report {
    uint32_t abi_version;
    uint32_t packets;
    uint32_t indirect_buffers;
    uint32_t register_writes;
    uint32_t draws;
    uint32_t ready_draws;
    uint32_t rejected_draws;
    uint64_t checksum;
} MisakiPM424Report;

// 0 success, -1 invalid args, -2 decoding error, -3 insufficient capacity,
// -4 other failure; no partial writes to the draw output on errors.
int32_t misaki_pm424_diagnostic(MisakiPM424Draw *output, size_t capacity,
                               MisakiPM424Report *report);
#ifdef __cplusplus
}
#endif
#endif
