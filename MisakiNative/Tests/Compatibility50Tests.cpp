#include "GuestSCE50.hpp"
#include "MisakiMega50Bridge.h"
#include <cassert>
#include <iostream>
using namespace misaki;
int main(){
    const auto a=runSCE50Diagnostic();
    assert(a);
    for(bool stage:a->stages)assert(stage);
    assert(a->segments==2 && a->relocations==1 && a->relocationValue==0x103500);
    assert(a->modules==2 && a->imports==2 && a->guestRax==42);
    assert(a->mappedGuestPages==1 && a->serviceCalls==4 && a->serviceFailures==0);
    assert(a->pm4Packets==11 && a->sprites==2 && a->framebufferChecksum!=0);
    assert(a->completedFence==1 && a->firmwareBlockers==5);
    assert(!a->firmwareAttempted && !a->homeMenuBooted);
    MisakiMega50Report bridge{};
    assert(misaki_mega50_diagnostic(&bridge)==0);
    assert(bridge.abi_version==1 && bridge.pass_46 && bridge.pass_47 && bridge.pass_48 &&
           bridge.pass_49 && bridge.pass_50 && bridge.firmware_blockers==5);
    assert(bridge.framebuffer_checksum==a->framebufferChecksum);
    assert(misaki_mega50_diagnostic(nullptr)==-1);
    const auto b=runSCE50Diagnostic();
    assert(b && b->framebufferChecksum==a->framebufferChecksum);
    std::cout<<"PASS: compatibility 50 single-context guest system integration and boot gate\n";
}
