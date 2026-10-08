#ifndef MISAKI_CORE_BRIDGE_H
#define MISAKI_CORE_BRIDGE_H

#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

// Stable C-only interface, callable from Swift without an Objective-C++ wrapper.
typedef struct MisakiCoreReport {
    uint32_t abi_version;
    uint32_t elf_type;
    uint32_t load_segments;
    uint32_t registered_modules;
    uint32_t resolved_imports;
    uint32_t import_is_read_only;
    uint64_t linked_guest_address;
} MisakiCoreReport;

// Returns 0 on success, negative on failure. No PS4 firmware is executed.
int32_t misaki_core_run_diagnostic(MisakiCoreReport *report);
const char *misaki_core_version(void);

// Validates general x86-64 ELF64 program headers without running instructions.
int32_t misaki_core_inspect_elf(const uint8_t *bytes, size_t length,
                               uint32_t *type, uint32_t *load_segment_count);

// Native instruction-execution diagnostic. The guest ELF and library are
// self-authored fixtures, not PlayStation firmware or Sony libraries.
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

// Returns 0 only if the guest diagnostic ran to completion and verified RAX=42.
int32_t misaki_core_run_cpu_diagnostic(MisakiNativeCPUReport *report);

#ifdef __cplusplus
} // extern "C"
#endif
#endif
