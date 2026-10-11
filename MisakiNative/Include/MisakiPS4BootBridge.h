#ifndef MISAKI_PS4_BOOT_BRIDGE_H
#define MISAKI_PS4_BOOT_BRIDGE_H
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif
// Reports PS4-format *inspection* plus synthetic execution tests, NEVER a
// genuine firmware boot or an implemented SCE syscall/SELF decryptor.
typedef struct MisakiPS4BootReport {
    uint32_t abi_version;
    uint32_t passed_36;
    uint32_t passed_37;
    uint32_t passed_38;
    uint32_t passed_39;
    uint32_t passed_40; // true means firmware launch was CORRECTLY BLOCKED
    uint32_t synthetic_self_segments;
    uint32_t encrypted_segments;
    uint32_t sce_elf_type;
    uint32_t resolved_synthetic_imports;
    uint32_t cooperative_gcn_lanes;
    uint32_t gcn_instructions;
    uint32_t processes;
    uint32_t x64_instructions;
    uint32_t pm4_packets;
    uint32_t framebuffer_sprites;
    uint32_t readiness_blockers;
    uint32_t firmware_execution_attempted;
    uint32_t home_menu_booted;
    uint64_t completed_gpu_fence;
} MisakiPS4BootReport;
// 0 validated diagnostic including negative/blocked real-boot status;
// -1 null report; -2 incomplete native diagnostic; -3 invalid boot gate.
int32_t misaki_ps4_boot_readiness_diagnostic(MisakiPS4BootReport *report);
#ifdef __cplusplus
}
#endif
#endif
