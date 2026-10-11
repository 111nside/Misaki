#ifndef MISAKI_METAL_IR_BRIDGE_H
#define MISAKI_METAL_IR_BRIDGE_H
#include <stddef.h>
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif

// The returned source is a self-authored restricted Metal COMPUTE shader.
// It is not guest-provided arbitrary text or a PS4 GPU shader binary.
typedef struct MisakiMetalIR27Report {
    uint32_t abi_version;
    uint32_t ir_instructions;
    uint32_t alu_statements;
    uint32_t source_bytes; // including NUL terminator
    uint32_t sgpr1;
    uint32_t sgpr2;
    uint32_t sgpr3;
    uint32_t vgpr0_bits;
    uint32_t vgpr1_bits;
    uint32_t vgpr2_bits;
    uint64_t ir_checksum;
    uint64_t metal_source_hash;
    uint64_t software_result_checksum;
} MisakiMetalIR27Report;

// 0 success; -1 bad pointer; -2 unsupported IR; -3 insufficient capacity;
// -4 unexpected error. Source and report are written only after validation;
// report is cleared on non-argument errors. No partial source is copied.
int32_t misaki_metal_ir27_shader(char *output, size_t capacity,
                                 MisakiMetalIR27Report *report);

#ifdef __cplusplus
}
#endif
#endif
