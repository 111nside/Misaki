#include "../Include/MisakiCoreBridge.h"
#include "../Include/MisakiCore.hpp"
#include "../Include/GuestCPU.hpp"
#include "../Include/GuestDynamicExecution.hpp"
#include "../Include/GuestServices.hpp"
#include "../Include/GuestProcesses.hpp"
#include "../Include/GuestX64Backend.hpp"

extern "C" {
int32_t misaki_core_run_process_diagnostic(MisakiProcessReport *report) {
    if (!report) return -1;
    *report = MisakiProcessReport{};
    try {
        const auto run = misaki::runGuestProcessDiagnostic();
        if (!run || run->results.size() != 2) return -2;
        const auto &a = run->results[0];
        const auto &b = run->results[1];
        report->abi_version = 1;
        report->process_count = 2;
        report->instructions = run->instructions;
        report->yields = run->yields;
        report->opens = run->opens;
        report->reads = run->reads;
        report->closes = run->closes;
        report->writes = run->writes;
        report->output_bytes = static_cast<uint32_t>(run->output.size());
        report->output_matches = run->output == "HelloHello" ? 1u : 0u;
        report->stacks_restored = a.stackRestored() && b.stackRestored() ? 1u : 0u;
        report->pid1_rax = a.rax();
        report->pid2_rax = b.rax();
        const bool passed = run->completed && run->pids.size() == 2 &&
                            a.stop == misaki::X64Stop::halted &&
                            b.stop == misaki::X64Stop::halted &&
                            report->instructions == 44 && report->yields == 2 &&
                            report->opens == 2 && report->reads == 2 &&
                            report->closes == 2 && report->writes == 2 &&
                            report->output_bytes == 10 && report->output_matches == 1 &&
                            report->stacks_restored == 1 &&
                            report->pid1_rax == 1001 && report->pid2_rax == 1002;
        return passed ? 0 : -3;
    } catch (...) {
        return -4;
    }
}


int32_t misaki_core_run_services_diagnostic(MisakiServiceThreadReport *report) {
    if (!report) return -1;
    *report = MisakiServiceThreadReport{};
    try {
        const auto run = misaki::runGuestServiceDiagnostic();
        if (!run || run->scheduler.threadResults.size() != 2) return -2;
        const auto &a = run->scheduler.threadResults[0];
        const auto &b = run->scheduler.threadResults[1];
        report->abi_version = 1;
        report->thread_count = 2;
        report->instructions = run->scheduler.instructions;
        report->yields = run->scheduler.yieldEvents;
        report->service_calls = run->calls;
        report->write_calls = run->writes;
        report->output_bytes = static_cast<uint32_t>(run->output.size());
        report->output_matches = run->output == "OKOK" ? 1u : 0u;
        report->threads_halted = a.stop == misaki::X64Stop::halted &&
                                 b.stop == misaki::X64Stop::halted ? 1u : 0u;
        report->stacks_restored = a.stackRestored() && b.stackRestored() ? 1u : 0u;
        report->thread1_rax = a.rax();
        report->thread2_rax = b.rax();
        const bool passed = run->scheduler.completed &&
                            report->instructions == 30 && report->yields == 2 &&
                            report->service_calls == 8 && report->write_calls == 2 &&
                            report->output_bytes == 4 && report->output_matches == 1 &&
                            report->threads_halted && report->stacks_restored &&
                            a.rax() == 4097 && b.rax() == 4098;
        return passed ? 0 : -3;
    } catch (...) {
        return -4;
    }
}


int32_t misaki_core_run_dynamic_cpu_diagnostic(MisakiDynamicCPUReport *report) {
    if (!report) return -1;
    *report = MisakiDynamicCPUReport{};
    try {
        const auto run = misaki::runDynamicCPUBackendDiagnostic();
        if (!run) return -2;
        report->abi_version = 1;
        report->loaded_segments = run->linked.mappedSegments;
        report->imports = run->linked.importedSymbols;
        report->relative_relocations = run->linked.relativeRelocations;
        report->instructions = run->execution.state.instructions;
        report->halted = run->execution.stop == misaki::X64Stop::halted ? 1u : 0u;
        report->stack_restored = run->execution.stackRestored() ? 1u : 0u;
        report->import_read_only = run->importReadOnly ? 1u : 0u;
        report->rax = run->execution.rax();
        report->entry = run->linked.entry;
        report->linked_address = run->linked.firstImportTarget;
        report->relative_value = run->relativeValue;
        const bool passed = report->abi_version == 1 && report->loaded_segments == 2 &&
                            report->imports == 1 && report->relative_relocations == 1 &&
                            report->instructions == 5 && report->halted &&
                            report->stack_restored && report->import_read_only &&
                            report->rax == 42 && report->entry == 0x5000 &&
                            report->linked_address == 0x9000 && report->relative_value == 0x5234;
        return passed ? 0 : -3;
    } catch (...) {
        return -4;
    }
}


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

const char *misaki_core_version(void) { return "0.14.0-guest-processes"; }

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
