#ifndef MISAKI_MUBUF_BRIDGE_H
#define MISAKI_MUBUF_BRIDGE_H
#include <stddef.h>
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif

// Milestone 29: actual GCN 1.1 MUBUF word decoding (restricted operation
// subset), one-lane reference emulation and generated Metal resource kernel.
// Not PS4 shader or wavefront execution.
typedef struct MisakiMUBUF29Report {
    uint32_t abi_version;
    uint32_t decoded_instructions;
    uint32_t load_opcode;
    uint32_t store_opcode;
    uint32_t first_vaddr;
    uint32_t first_vdata;
    uint32_t input_srsrc;
    uint32_t output_srsrc;
    uint32_t record_index;
    uint32_t loaded_word;
    uint32_t stored_word;
    uint32_t source_bytes;
    uint32_t input_bytes;
    uint32_t output_words;
    uint32_t code_read_only;
    uint32_t descriptors_read_only;
    uint32_t input_read_only;
    uint32_t output_writable;
    uint64_t input_base;
    uint64_t output_base;
    uint64_t instruction_checksum;
    uint64_t metal_source_hash;
    uint64_t expected_output_hash;
} MisakiMUBUF29Report;

// All-or-nothing output. The valid report is cleared on any failure.
// 0 ok, -1 invalid argument, -2 diagnostic failed, -3 capacity, -4 exception.
int32_t misaki_mubuf29_plan(char *metal_source, size_t source_capacity,
    uint8_t *input_bytes, size_t input_capacity,
    uint32_t *output_initial, size_t initial_capacity,
    uint32_t *output_expected, size_t expected_capacity,
    MisakiMUBUF29Report *report);

#ifdef __cplusplus
}
#endif
#endif
