#ifndef MISAKI_GCN25_BRIDGE_H
#define MISAKI_GCN25_BRIDGE_H
#include <stddef.h>
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif

// GCN ISA metadata only. No GCN wave execution, Metal translation, or Sony code.
// encoding: 1 SOP2, 2 SOP1, 3 SOPK, 4 SOPP, 5 VOP2, 6 VOP1.
typedef struct MisakiGCN25Instruction {
    uint64_t guest_address;
    uint32_t word_offset;
    uint32_t encoding;
    uint32_t opcode;
    uint32_t dst;
    uint32_t src0;
    uint32_t src1;
    uint32_t literal;
    uint32_t length_words;
    uint32_t has_literal;
} MisakiGCN25Instruction;

typedef struct MisakiGCN25Report {
    uint32_t abi_version;
    uint32_t pm4_packets;
    uint32_t pm4_draws;
    uint32_t instruction_count;
    uint32_t scalar_instructions;
    uint32_t vector_instructions;
    uint32_t literal_words;
    uint32_t shader_words;
    uint32_t terminated;
    uint32_t shader_read_only;
    uint32_t descriptor_read_only;
    uint32_t descriptor_supported;
    uint32_t descriptor_stride;
    uint32_t descriptor_records;
    uint32_t descriptor_data_format;
    uint32_t descriptor_numeric_format;
    uint32_t descriptor_swizzled;
    uint64_t shader_address;
    uint64_t descriptor_base;
    uint64_t checksum;
} MisakiGCN25Report;

// 0 success, -1 arguments, -2 unsupported/bad diagnostic, -3 small output,
// -4 unexpected error. Never writes partial output on failure.
int32_t misaki_gcn25_diagnostic(MisakiGCN25Instruction *instructions,
                                size_t capacity, MisakiGCN25Report *report);
#ifdef __cplusplus
}
#endif
#endif
