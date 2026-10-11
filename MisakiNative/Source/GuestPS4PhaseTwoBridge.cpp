#include "../Include/MisakiPS4PhaseTwoBridge.h"
#include "../Include/GuestPS4PhaseTwo.hpp"
extern "C" int32_t misaki_ps4_phase_two_diagnostic(MisakiPS4PhaseTwoReport *r) {
    if (!r) return -1;
    *r=MisakiPS4PhaseTwoReport{};
    try {
        const auto p=misaki::runPS4PhaseTwoDiagnostic();
        if (!p) return -2;
        r->abi_version=1;
        r->passed_41=p->stagePassed[0];r->passed_42=p->stagePassed[1];
        r->passed_43=p->stagePassed[2];r->passed_44=p->stagePassed[3];
        r->passed_45=p->stagePassed[4];
        r->mapped_segments=p->mappedSegments;
        r->relative_relocations=p->relocations;
        r->imports=p->imports;
        r->service_calls=p->serviceCalls;
        r->instructions=p->guestInstructions;
        r->output_bytes=p->emittedBytes;
        r->guest_rax=p->guestResult;
        r->relocation_value=p->relocationValue;
        r->pm4_packets=p->pm4Packets;
        r->gpu_packets=p->gpuPackets;
        r->sprites=p->sprites;
        r->completed_fence=p->completedFence;
        r->framebuffer_checksum=p->framebufferChecksum;
        r->real_boot_blockers=p->bootBlockers;
        r->firmware_attempted=p->firmwareAttempted;
        r->home_menu_booted=p->homeMenuBooted;
        return r->passed_41 && r->passed_42 && r->passed_43 && r->passed_44 &&
               r->passed_45 && !r->firmware_attempted && !r->home_menu_booted &&
               r->real_boot_blockers==5 ? 0 : -3;
    } catch (...) { return -4; }
}
