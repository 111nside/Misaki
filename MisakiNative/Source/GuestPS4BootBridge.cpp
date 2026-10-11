#include "../Include/MisakiPS4BootBridge.h"
#include "../Include/GuestBootReadiness.hpp"
extern "C" int32_t misaki_ps4_boot_readiness_diagnostic(MisakiPS4BootReport *r) {
    if (!r) return -1;
    *r = MisakiPS4BootReport{};
    try {
        const auto d=misaki::runEarlyBootDiagnostic();
        if (!d || d->events.size()!=5) return -2;
        const auto gate=misaki::evaluatePS4BootReadiness(*d,misaki::PS4BootCapabilities{});
        r->abi_version=1;
        r->passed_36=d->events[0].passed?1u:0u;
        r->passed_37=d->events[1].passed?1u:0u;
        r->passed_38=d->events[2].passed?1u:0u;
        r->passed_39=d->events[3].passed?1u:0u;
        r->passed_40=(!gate.prerequisitesReported && !gate.firmwareExecutionAttempted &&
                      !gate.homeMenuBooted && gate.blockers.size()==5) ? 1u:0u;
        r->synthetic_self_segments=d->executable.selfSegments;
        r->encrypted_segments=d->executable.encryptedSegments;
        r->sce_elf_type=d->executable.elfType;
        r->resolved_synthetic_imports=d->resolvedImports;
        r->cooperative_gcn_lanes=d->shaderLanes;
        r->gcn_instructions=d->shaderOperations;
        r->processes=d->sessionProcesses;
        r->x64_instructions=d->sessionInstructions;
        r->pm4_packets=d->pm4Packets;
        r->framebuffer_sprites=d->frameSprites;
        r->readiness_blockers=static_cast<std::uint32_t>(gate.blockers.size());
        r->firmware_execution_attempted=gate.firmwareExecutionAttempted?1u:0u;
        r->home_menu_booted=gate.homeMenuBooted?1u:0u;
        r->completed_gpu_fence=d->lastFence;
        return r->passed_36 && r->passed_37 && r->passed_38 && r->passed_39 &&
               r->passed_40 && r->processes==2 ? 0 : -3;
    } catch (...) { return -2; }
}
