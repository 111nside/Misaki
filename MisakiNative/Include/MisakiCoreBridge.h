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

// ET_DYN + dynamic import integration: self-authored guest executable only.
// C-compatible ABI; no PS4 SELF, PRX, NID or firmware support is implied.
typedef struct MisakiDynamicCPUReport {
    uint32_t abi_version;
    uint32_t loaded_segments;
    uint32_t imports;
    uint32_t relative_relocations;
    uint32_t instructions;
    uint32_t halted;
    uint32_t stack_restored;
    uint32_t import_read_only;
    uint64_t rax;
    uint64_t entry;
    uint64_t linked_address;
    uint64_t relative_value;
} MisakiDynamicCPUReport;

int32_t misaki_core_run_dynamic_cpu_diagnostic(MisakiDynamicCPUReport *report);

// Milestone 13: original toy services + deterministic cooperative guest scheduler.
// These are NOT Sony, FreeBSD, or Darwin system calls.
typedef struct MisakiServiceThreadReport {
    uint32_t abi_version;
    uint32_t thread_count;
    uint32_t instructions;
    uint32_t yields;
    uint32_t service_calls;
    uint32_t write_calls;
    uint32_t output_bytes;
    uint32_t output_matches;
    uint32_t threads_halted;
    uint32_t stacks_restored;
    uint64_t thread1_rax;
    uint64_t thread2_rax;
} MisakiServiceThreadReport;

int32_t misaki_core_run_services_diagnostic(MisakiServiceThreadReport *report);

// Milestone 14: isolated in-memory guest process/VFS test ABI only.
// No guest file operation reaches the device's host filesystem.
typedef struct MisakiProcessReport {
    uint32_t abi_version;
    uint32_t process_count;
    uint32_t instructions;
    uint32_t yields;
    uint32_t opens;
    uint32_t reads;
    uint32_t closes;
    uint32_t writes;
    uint32_t output_bytes;
    uint32_t output_matches;
    uint32_t stacks_restored;
    uint64_t pid1_rax;
    uint64_t pid2_rax;
} MisakiProcessReport;

int32_t misaki_core_run_process_diagnostic(MisakiProcessReport *report);

// Milestone 15: self-authored ELF64 module linked to a guest service library.
// Uses a private demonstration ABI and a read-only, in-memory VFS, not PS4 APIs.
typedef struct MisakiSystemLibraryReport {
    uint32_t abi_version;
    uint32_t loaded_segments;
    uint32_t imported_symbols;
    uint32_t relative_relocations;
    uint32_t instructions;
    uint32_t halted;
    uint32_t stack_restored;
    uint32_t import_read_only;
    uint32_t library_read_only;
    uint32_t process_id;
    uint32_t opens;
    uint32_t reads;
    uint32_t closes;
    uint32_t writes;
    uint32_t output_matches;
    uint64_t rax;
    uint64_t entry;
    uint64_t linked_address;
} MisakiSystemLibraryReport;

int32_t misaki_core_run_system_library_diagnostic(MisakiSystemLibraryReport *report);

#ifdef __cplusplus
} // extern "C"
#endif
#endif
