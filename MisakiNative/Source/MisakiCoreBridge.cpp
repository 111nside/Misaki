#include "../Include/MisakiCoreBridge.h"
#include "../Include/MisakiCore.hpp"
#include "../Include/GuestCPU.hpp"
#include "../Include/GuestX64Backend.hpp"

extern "C" {

int32_t misaki_core_run_backend_diagnostic(MisakiX64BackendReport *report) {
    if (!report) return -1;
    *report = MisakiX64BackendReport{};
    try {
        const auto d = misaki::runX64BackendDiagnostic();
        if (!d) return -2;
        report->abi_version = 1;
        report->backend_id = 1; // portable interpreter, not a JIT
        report->instructions = d->execution.state.instructions;
        report->halted = d->execution.stop == misaki::X64Stop::halted ? 1u : 0u;
        report->stack_restored = d->execution.stackRestored() ? 1u : 0u;
        report->zero_flag = d->execution.state.zeroFlag ? 1u : 0u;
        report->resolved_imports = d->importedSymbols;
        report->import_read_only = d->importReadOnly ? 1u : 0u;
        report->rax = d->execution.rax();
        report->linked_address = d->linkedAddress;
        const bool pass = report->halted == 1 && report->stack_restored == 1 &&
                          report->zero_flag == 0 && report->rax == 44 &&
                          report->resolved_imports == 1 &&
                          report->import_read_only == 1 &&
                          report->linked_address == 0x3000 &&
                          report->instructions == 13;
        return pass ? 0 : -3;
    } catch (...) {
        return -4;
    }
}

int32_t misaki_core_run_cpu_diagnostic(MisakiNativeCPUReport *report) {
    if (!report) return -1;
    *report = MisakiNativeCPUReport{};
    try {
        const auto diagnostic = misaki::runGuestCPUDiagnostic();
        if (!diagnostic) return -2;
        report->abi_version = 1;
        report->loaded_segments = diagnostic->segments;
        report->resolved_imports = diagnostic->imports;
        report->instructions = diagnostic->cpu.instructions;
        report->halted = diagnostic->cpu.stop == misaki::GuestStop::halted ? 1u : 0u;
        report->stack_restored = diagnostic->cpu.stackRestored ? 1u : 0u;
        report->import_read_only = diagnostic->importReadOnly ? 1u : 0u;
        report->rax = diagnostic->cpu.rax;
        report->linked_address = diagnostic->linkedAddress;
        const bool passed = report->halted && report->rax == 42 &&
                            report->instructions == 5 && report->stack_restored &&
                            report->loaded_segments == 2 && report->resolved_imports == 1 &&
                            report->linked_address == 0x3000 && report->import_read_only;
        return passed ? 0 : -3;
    } catch (...) {
        return -4;
    }
}

const char *misaki_core_version(void) { return "0.11.0-portable-backend"; }

int32_t misaki_core_run_diagnostic(MisakiCoreReport *report) {
    if (!report) return -1;
    *report = MisakiCoreReport{};
    try {
        const auto result = misaki::runDiagnostic();
        if (!result) return -2;
        report->abi_version = 1;
        report->elf_type = result->elf.type;
        report->load_segments = result->elf.loadSegments;
        report->registered_modules = result->loadedModules;
        report->resolved_imports = result->importsResolved;
        report->import_is_read_only = result->importReadOnly ? 1u : 0u;
        report->linked_guest_address = result->linkedAddress;
        return 0;
    } catch (...) {
        return -3;
    }
}

int32_t misaki_core_inspect_elf(const uint8_t *bytes, size_t length,
                                uint32_t *type, uint32_t *load_segment_count) {
    if (!type || !load_segment_count) return -1;
    *type = 0;
    *load_segment_count = 0;
    try {
        const auto info = misaki::inspectELF64(bytes, length);
        if (!info) return -2;
        *type = info->type;
        *load_segment_count = info->loadSegments;
        return 0;
    } catch (...) {
        return -3;
    }
}
} // extern "C"
