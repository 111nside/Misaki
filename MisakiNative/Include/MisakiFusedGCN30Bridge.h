#ifndef MISAKI_FUSED_GCN30_BRIDGE_H
#define MISAKI_FUSED_GCN30_BRIDGE_H
#include <stddef.h>
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif

// Milestone 30: one fused GCN1.1 ALU/MUBUF instruction stream translated to
// one Metal compute kernel. Diagnostic-only; not general PS4 shader execution.
typedef struct MisakiFusedGCN30Report {
    uint32_t abi_version;
    uint32_t instructions;
    uint32_t code_words;
    uint32_t alu_operations;
    uint32_t buffer_loads;
    uint32_t buffer_stores;
    uint32_t executed;
    uint32_t record_index;
    uint32_t loaded;
    uint32_t stored;
    uint32_t source_bytes;
    uint32_t input_bytes;
    uint32_t output_words;
    uint32_t register_words;
    uint32_t code_read_only;
    uint32_t descriptors_read_only;
    uint32_t input_read_only;
    uint32_t output_writable;
    uint64_t input_base;
    uint64_t output_base;
    uint64_t instruction_checksum;
    uint64_t metal_source_hash;
    uint64_t expected_output_hash;
} MisakiFusedGCN30Report;

// Status 0=success, -1=invalid parameters, -2=diagnostic failure,
// -3=output capacity, -4=exception. No output arrays are partially written.
int32_t misaki_gcn30_plan(char *source, size_t source_capacity,
    uint8_t *input, size_t input_capacity,
    uint32_t *initial, size_t initial_capacity,
    uint32_t *expected, size_t expected_capacity,
    uint32_t *registers, size_t register_capacity,
    MisakiFusedGCN30Report *report);
#ifdef __cplusplus
}
#endif
#endif
