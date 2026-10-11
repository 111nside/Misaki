#ifndef MISAKI_MEGA60_BRIDGE_H
#define MISAKI_MEGA60_BRIDGE_H
#include <stddef.h>
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif
typedef struct MisakiMega60Report {
    uint32_t abi_version;
    uint32_t pass_56,pass_57,pass_58,pass_59,pass_60;
    uint32_t elf_dependencies,rela_records,service_calls,service_failures;
    uint32_t gpu_pixels,gpu_copies,guest_processes,guest_exits;
    uint32_t guest_instructions,context_switches,encrypted_self_segments;
    uint32_t firmware_blockers,analysis_refused_execution;
    uint32_t firmware_attempted,home_menu_booted;
} MisakiMega60Report;
int32_t misaki_mega60_diagnostic(MisakiMega60Report *report);
typedef struct MisakiCompatibility60Report {
    uint32_t abi_version,kind,elf_type,load_segments,dynamic_segments;
    uint32_t encrypted_segments,compressed_segments,needed_modules,findings;
    uint32_t partial,recognized,execution_refused;
    uint64_t inspected_bytes,file_bytes;
} MisakiCompatibility60Report;
// Returns 0 for a completed, READ-ONLY classification (even if unsupported).
// Never decrypts, writes to an input file, loads executable code or boots firmware.
// On insufficient text capacity, report/text are not partially written.
// Total file length must be >= supplied prefix length and <= UINT64_MAX.
int32_t misaki_compat60_inspect(const uint8_t *bytes,size_t prefix_length,
       uint64_t actual_length,MisakiCompatibility60Report *report,
       char *diagnostic,size_t diagnostic_capacity);
#ifdef __cplusplus
}
#endif
#endif
