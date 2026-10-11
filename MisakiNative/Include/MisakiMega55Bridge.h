#ifndef MISAKI_MEGA55_BRIDGE_H
#define MISAKI_MEGA55_BRIDGE_H
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif
// Stable C ABI; all reported passes apply to synthetic diagnostics only.
typedef struct MisakiMega55Report {
    uint32_t abi_version;
    uint32_t pass_51, pass_52, pass_53, pass_54, pass_55;
    uint32_t elf_segments, elf_relocations, self_encrypted_segments;
    uint32_t sce_dynamic_tags, synthetic_nids, processes, mapped_pages;
    uint32_t pm4_packets, gcn_lanes, gcn_instructions, output_index_7;
    uint64_t guest_rax, shader_hash;
    uint32_t encrypted_self_refused, firmware_blockers;
    uint32_t firmware_attempted, home_menu_booted;
} MisakiMega55Report;
// 0 completed diagnostic, -1 invalid args, -2 internal diagnostic failure,
// -3 a known expected invariant failed. Never passes through guest pointers.
int32_t misaki_mega55_diagnostic(MisakiMega55Report *report);
#ifdef __cplusplus
}
#endif
#endif
