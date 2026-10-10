#ifndef MISAKI_MODULE_LIFECYCLE_BRIDGE_H
#define MISAKI_MODULE_LIFECYCLE_BRIDGE_H

#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif

// Milestone 17. Synthetic guest module lifecycle, not PS4 PRX/SCE APIs.
typedef struct MisakiModuleLifecycleReport {
    uint32_t abi_version;
    uint32_t modules_loaded;
    uint32_t linked_imports;
    uint32_t instructions;
    uint32_t initializations;
    uint32_t finalizations;
    uint32_t stack_restored;
    uint32_t import_read_only;
    uint32_t unloaded;
    uint32_t dependency_order_valid;
    uint64_t rax;
} MisakiModuleLifecycleReport;

int32_t misaki_core_run_lifecycle_diagnostic(MisakiModuleLifecycleReport *report);

#ifdef __cplusplus
}
#endif
#endif
