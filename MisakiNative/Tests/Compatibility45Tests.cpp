#include "GuestPS4PhaseTwo.hpp"
#include <cstdlib>
#include <iostream>
using namespace misaki;
namespace {
unsigned count=0;
void check(bool v,const char*s){++count;if(!v){std::cerr<<"FAIL 45: "<<s<<'\n';std::exit(1);}}
}
int main(){
    const auto result=runPS4PhaseTwoDiagnostic();
    check(bool(result),"combined staging/linking/services/gpu diagnostic");
    for(unsigned i=0;i<5;++i) check(result->stagePassed[i],"all five stages completed");
    check(result->mappedSegments==1 && result->relocations==1,"SCE staging performed");
    check(result->relocationValue==0x102000,"relative relocation patched");
    check(result->imports==2,"NID-like import slots bound");
    check(result->serviceCalls==3 && result->emittedBytes==5,"guest kernel requests executed");
    check(result->guestResult==42 && result->guestInstructions>0,"guest imported RX entry executed");
    check(result->pm4Packets==11 && result->gpuPackets==10,"PM4 and guest framebuffer integrated");
    check(result->sprites==2 && result->completedFence==1,"two sprite frame and fence");
    check(result->framebufferChecksum!=0,"computed surface checksum");
    check(result->bootBlockers==5,"real boot correctly blocked");
    check(!result->firmwareAttempted && !result->homeMenuBooted,"no fabricated boot success");
    const auto again=runPS4PhaseTwoDiagnostic();
    check(bool(again),"deterministic second run");
    check(again->framebufferChecksum==result->framebufferChecksum,"stable surface checksum");
    check(again->guestInstructions==result->guestInstructions,"stable CPU instructions");
    std::cout<<"PASS: "<<count<<" milestone 45 integrated boot-gate assertions\n";
}
