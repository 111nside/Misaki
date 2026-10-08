#ifndef MISAKI_CORE_BRIDGE_H
#define MISAKI_CORE_BRIDGE_H

#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct MisakiCoreReport {
    uint32_t abi_version;
    uint32_t elf_type;
    uint32_t load_segments;
    uint32_t registered_modules;
    uint32_t resolved_imports;
    uint32_t import_is_read_only;
    uint64_t linked_guest_address;
} MisakiCoreReport;

int32_t misaki_core_run_diagnostic(MisakiCoreReport *report);
const char *misaki_core_version(void);
int32_t misaki_core_inspect_elf(const uint8_t *bytes, size_t length,
                               uint32_t *type, uint32_t *load_segment_count);

typedef struct MisakiNativeCPUReport {
    uint32_t abi_version;
    uint32_t loaded_segments;
    uint32_t resolved_imports;
    uint32_t instructions;
    uint32_t halted;
    uint32_t stack_restored;
    uint32_t import_read_only;
    uint64_t rax;
    uint64_t linked_address;
} MisakiNativeCPUReport;

int32_t misaki_core_run_cpu_diagnostic(MisakiNativeCPUReport *report);

// Milestone 11: an independent, C++17 x86-64 interpreter backend.
// These are synthetic guest tests, not PS4 firmware or a commercial game.
typedef struct MisakiX64BackendReport {
    uint32_t abi_version;
    uint32_t backend_id;
    uint32_t instructions;
    uint32_t halted;
    uint32_t stack_restored;
    uint32_t zero_flag;
    uint32_t resolved_imports;
    uint32_t import_read_only;
    uint64_t rax;
    uint64_t linked_address;
} MisakiX64BackendReport;

int32_t misaki_core_run_backend_diagnostic(MisakiX64BackendReport *report);

#ifdef __cplusplus
} // extern "C"
#endif
#endif
