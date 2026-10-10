#ifndef MISAKI_PM4_BRIDGE_H
#define MISAKI_PM4_BRIDGE_H

#include <stddef.h>
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif

// Portable C ABI exposing an early AMD GCN PM4 packet *trace*.
// These are never interpreted as host/PS4 GPU instructions.
typedef struct MisakiPM4RegisterWrite {
    uint32_t address;
    uint32_t value;
    uint32_t packet_index;
    uint32_t shader_type;
} MisakiPM4RegisterWrite;

typedef struct MisakiPM4TraceReport {
    uint32_t abi_version;
    uint32_t packet_count;
    uint32_t type0_packets;
    uint32_t type2_packets;
    uint32_t type3_packets;
    uint32_t nop_packets;
    uint32_t register_writes;
    uint32_t context_writes;
    uint32_t shader_writes;
    uint32_t other_writes;
    uint32_t draw_auto_packets;
    uint32_t event_writes;
    uint32_t last_vertex_count;
    uint32_t guest_memory_verified;
    uint64_t checksum;
} MisakiPM4TraceReport;

// 0 success, -1 invalid arguments, -2 malformed/unsupported packet,
// -3 insufficient output capacity, -4 other failure.
// A failure never partially writes the supplied register array.
int32_t misaki_pm4_decode_words(const uint32_t *words, size_t word_count,
                                MisakiPM4RegisterWrite *writes, size_t capacity,
                                MisakiPM4TraceReport *report);

// Uses the above authentic packet framing with synthetic values. The buffer is
// mapped inside isolated guest memory, and never accesses the host GPU.
int32_t misaki_pm4_demo_trace(MisakiPM4RegisterWrite *writes, size_t capacity,
                              MisakiPM4TraceReport *report);

#ifdef __cplusplus
}
#endif
#endif
