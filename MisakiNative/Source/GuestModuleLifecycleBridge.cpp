#include "../Include/MisakiModuleLifecycleBridge.h"
#include "../Include/GuestModuleLifecycle.hpp"

extern "C" int32_t misaki_core_run_lifecycle_diagnostic(MisakiModuleLifecycleReport *report) {
    if (!report) return -1;
    *report = MisakiModuleLifecycleReport{};
    try {
        const auto run = misaki::runGuestModuleLifecycleDiagnostic();
        if (!run) return -2;
        report->abi_version = 1;
        report->modules_loaded = run->modulesLoaded;
        report->linked_imports = run->imports;
        report->instructions = run->instructions;
        report->initializations = run->initializations;
        report->finalizations = run->finalizations;
        report->stack_restored = run->stackRestored ? 1u : 0u;
        report->import_read_only = run->importReadOnly ? 1u : 0u;
        report->unloaded = run->unloaded ? 1u : 0u;
        report->dependency_order_valid = run->dependencyProtected ? 1u : 0u;
        report->rax = run->rax;
        const bool pass = report->modules_loaded == 3 && report->linked_imports == 1 &&
                          report->instructions == 5 && report->initializations == 3 &&
                          report->finalizations == 3 && report->stack_restored &&
                          report->import_read_only && report->unloaded &&
                          report->dependency_order_valid && report->rax == 42;
        return pass ? 0 : -3;
    } catch (...) {
        return -4;
    }
}
