#ifndef MISAKI_SHADER_IR_BRIDGE_H
#define MISAKI_SHADER_IR_BRIDGE_H
#include <stddef.h>
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif

// Versioned, memory-safe IR diagnostics. Source kinds: 0 none, 1 SGPR,
// 2 VGPR, 3 immediate u32 bit pattern. Ops match GuestShaderIR.hpp.
typedef struct MisakiShaderIRInstruction {
    uint64_t guest_address;
    uint32_t word_offset;
    uint32_t op;
    uint32_t destination;
    uint32_t src0_kind;
    uint32_t src0_value;
    uint32_t src1_kind;
    uint32_t src1_value;
} MisakiShaderIRInstruction;

typedef struct MisakiShaderIRReport {
    uint32_t abi_version;
    uint32_t pm4_packets;
    uint32_t pm4_draws;
    uint32_t ir_instructions;
    uint32_t scalar_instructions;
    uint32_t vector_instructions;
    uint32_t executed_instructions;
    uint32_t scalar_writes;
    uint32_t vector_writes;
    uint32_t terminated;
    uint32_t source_read_only;
    uint32_t descriptor_read_only;
    uint32_t sgpr1;
    uint32_t sgpr2;
    uint32_t sgpr3;
    uint32_t vgpr0_bits;
    uint32_t vgpr1_bits;
    uint32_t vgpr2_bits;
    uint64_t shader_address;
    uint64_t source_checksum;
    uint64_t ir_checksum;
    uint64_t result_checksum;
} MisakiShaderIRReport;

// 0 success, -1 invalid arguments, -2 diagnostic not supported,
// -3 insufficient output capacity, -4 exception. Never partially writes
// the caller's instruction buffer if capacity is insufficient.
int32_t misaki_shader_ir_demo(MisakiShaderIRInstruction *output,
                              size_t capacity, MisakiShaderIRReport *report);

#ifdef __cplusplus
}
#endif
#endif
