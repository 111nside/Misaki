#ifndef MISAKI_PS4_PHASE_TWO_BRIDGE_H
#define MISAKI_PS4_PHASE_TWO_BRIDGE_H
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif
// PASS for stage 45 means the synthetic integration worked while authentic
// PS4 firmware boot was correctly BLOCKED. Not a firmware launch API.
typedef struct MisakiPS4PhaseTwoReport {
    uint32_t abi_version;
    uint32_t passed_41, passed_42, passed_43, passed_44, passed_45;
    uint32_t mapped_segments, relative_relocations, imports;
    uint32_t service_calls, instructions, output_bytes;
    uint64_t guest_rax, relocation_value;
    uint32_t pm4_packets, gpu_packets, sprites;
    uint64_t completed_fence, framebuffer_checksum;
    uint32_t real_boot_blockers, firmware_attempted, home_menu_booted;
} MisakiPS4PhaseTwoReport;
int32_t misaki_ps4_phase_two_diagnostic(MisakiPS4PhaseTwoReport *report);
#ifdef __cplusplus
}
#endif
#endif
