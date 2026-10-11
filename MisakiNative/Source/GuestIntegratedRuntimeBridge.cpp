#include "../Include/MisakiIntegratedRuntimeBridge.h"
#include "../Include/GuestIntegratedRuntime.hpp"
#include <algorithm>

extern "C" int32_t misaki_integrated_runtime_demo(
    uint8_t *pixels, size_t capacity, MisakiIntegratedRuntimeReport *report) {
    if (!report || !pixels) return -1;
    *report = MisakiIntegratedRuntimeReport{};
    try {
        const auto run = misaki::runIntegratedMegaDiagnostic();
        if (!run || !run->stage31 || !run->stage32 || !run->stage33 ||
            !run->stage34 || !run->stage35) return -2;
        const auto &app = run->runtime.processes.at(0);
        const auto &elf = run->runtime.processes.at(1);
        if (capacity < app.framebuffer.size()) return -3;
        std::copy(app.framebuffer.begin(), app.framebuffer.end(), pixels);
        report->abi_version = 1;
        report->passed_31 = run->stage31;
        report->passed_32 = run->stage32;
        report->passed_33 = run->stage33;
        report->passed_34 = run->stage34;
        report->passed_35 = run->stage35;
        report->process_count = static_cast<uint32_t>(run->runtime.processes.size());
        report->instructions = run->runtime.instructions;
        report->context_switches = run->runtime.contextSwitches;
        report->vfs_opens = app.opens;
        report->vfs_reads = app.reads;
        report->vfs_closes = app.closes;
        report->gpu_calls = app.gpuCalls;
        report->pm4_packets = app.pm4Packets;
        report->shader_operations = app.shaderExecuted;
        report->loaded_modules = elf.loadedModules;
        report->resolved_imports = elf.resolvedImports;
        report->framebuffer_sprites = app.framebufferSprites;
        report->framebuffer_width = 320;
        report->framebuffer_height = 180;
        report->framebuffer_checksum = app.framebufferChecksum;
        report->submitted_fence = app.submittedFence;
        report->completed_fence = app.completedFence;
        report->elf_rax = elf.execution.rax();
        report->guest_pid = app.execution.rax();
        report->output_record_7 = app.shaderStored;
        report->bytes_written = static_cast<uint32_t>(app.framebuffer.size());
        return 0;
    } catch (...) { return -2; }
}
