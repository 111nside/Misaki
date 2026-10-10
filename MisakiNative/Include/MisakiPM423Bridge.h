#ifndef MISAKI_PM4_23_BRIDGE_H
#define MISAKI_PM4_23_BRIDGE_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

// Read-only C ABI. Does not execute guest GPU commands or use a host graphics API.
typedef struct MisakiPM423Packet {
    uint64_t guest_address;
    uint32_t type;
    uint32_t opcode;
    uint32_t depth;
    uint32_t body_words;
} MisakiPM423Packet;

typedef struct MisakiPM423Report {
    uint32_t abi_version;
    uint32_t packet_count;
    uint32_t indirect_buffers;
    uint32_t indirect_const_buffers;
    uint32_t maximum_depth;
    uint32_t guest_words;
    uint32_t register_writes;
    uint32_t draw_auto_packets;
    uint32_t event_writes;
    uint32_t observed_register_mask;
    uint32_t render_target_metadata_observed;
    uint32_t color0_base;
    uint32_t color0_pitch;
    uint32_t color0_info;
    uint32_t color_target_mask;
    uint32_t scissor_tl;
    uint32_t scissor_br;
    uint32_t pixel_shader_low;
    uint32_t pixel_shader_high;
    uint64_t checksum;
} MisakiPM423Report;

// 0 success, -1 invalid args, -2 invalid/unsupported or inaccessible PM4,
// -3 packet output too small, -4 other failure. Output stays unchanged on failure.
int32_t misaki_pm423_diagnostic(MisakiPM423Packet *output, size_t capacity,
                               MisakiPM423Report *report);

#ifdef __cplusplus
} // extern "C"
#endif
#endif
