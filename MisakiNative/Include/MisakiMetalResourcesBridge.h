#ifndef MISAKI_METAL_RESOURCES_BRIDGE_H
#define MISAKI_METAL_RESOURCES_BRIDGE_H
#include <stddef.h>
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif

// M28 resource-stage ABI: diagnostic fixture only, NOT AMD MUBUF translation.
// 0 success; -1 bad pointer; -2 reference failure; -3 capacity; -4 exception.
// All output buffers are unchanged on error; report is cleared when valid.
typedef struct MisakiMetalResource28Report {
    uint32_t abi_version;
    uint32_t source_bytes;
    uint32_t input_bytes;
    uint32_t output_words;
    uint32_t resource_ops;
    uint32_t descriptor_stride;
    uint32_t descriptor_records;
    uint32_t read_index;
    uint32_t write_index;
    uint32_t loaded_word;
    uint32_t stored_word;
    uint32_t guest_input_read_only;
    uint32_t guest_output_writable;
    uint64_t descriptor_base;
    uint64_t ir_checksum;
    uint64_t metal_source_hash;
    uint64_t output_initial_hash;
    uint64_t output_expected_hash;
} MisakiMetalResource28Report;

int32_t misaki_metal_resource28_plan(
    char *source, size_t source_capacity,
    uint8_t *input_bytes, size_t input_capacity,
    uint32_t *initial_words, size_t initial_capacity,
    uint32_t *expected_words, size_t expected_capacity,
    MisakiMetalResource28Report *report);

#ifdef __cplusplus
}
#endif
#endif
