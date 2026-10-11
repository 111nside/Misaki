#include "../Include/MisakiMega50Bridge.h"
#include "../Include/GuestSCE50.hpp"
extern "C" int32_t misaki_mega50_diagnostic(MisakiMega50Report *r) {
    if(!r)return -1;
    *r=MisakiMega50Report{};
    try {
        const auto d=misaki::runSCE50Diagnostic();
        if(!d)return -2;
        r->abi_version=1;
        r->pass_46=d->stages[0];r->pass_47=d->stages[1];
        r->pass_48=d->stages[2];r->pass_49=d->stages[3];
        r->pass_50=d->stages[4];
        r->segments=d->segments;r->relocations=d->relocations;
        r->modules=d->modules;r->imports=d->imports;
        r->relocation_value=d->relocationValue;r->guest_rax=d->guestRax;
        r->vm_pages=d->mappedGuestPages;
        r->service_calls=d->serviceCalls;r->service_failures=d->serviceFailures;
        r->pm4_packets=d->pm4Packets;r->sprites=d->sprites;
        r->framebuffer_checksum=d->framebufferChecksum;
        r->completed_fence=d->completedFence;
        r->firmware_blockers=d->firmwareBlockers;
        r->firmware_attempted=d->firmwareAttempted;
        r->home_menu_booted=d->homeMenuBooted;
        return r->pass_46 && r->pass_47 && r->pass_48 && r->pass_49 && r->pass_50 &&
               r->firmware_blockers==5 && !r->firmware_attempted && !r->home_menu_booted
               ? 0 : -3;
    } catch (...) {return -4;}
}
