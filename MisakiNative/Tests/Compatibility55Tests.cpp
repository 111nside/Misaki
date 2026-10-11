#include "GuestSCE55.hpp"
#include "MisakiMega55Bridge.h"
#include <iostream>
#include <cstdlib>
using namespace misaki;
static int checks=0;
static void check(bool ok,const char *why){++checks;if(!ok){std::cerr<<"FAIL M55: "<<why<<'\n';std::exit(1);}}
int main(){
    auto run=runSCE55Diagnostic();
    check(bool(run),"combined compatibility pipeline");
    for(bool passed:run->stages)check(passed,"every stage pass");
    check(run->elfSegments==2 && run->elfRelocations==1,"ELF staging/relocation");
    check(run->selfEncryptedSegments==1 && run->encryptedSelfRefused,"SELF refused as firmware");
    check(run->sceDynamicTags==3 && run->syntheticNids==2,"dynamic/NID metadata");
    check(run->processes==1 && run->mappedPages==2,"process/memory accounting");
    check(run->pm4Packets==11 && run->gcnLanes==4 && run->gcnInstructions==44,"PM4/GCN pipeline");
    check(run->outputIndex7==31 && run->shaderHash!=0,"GPU buffers verified");
    check(run->guestRax==42,"ELF execution still works");
    check(run->firmwareBlockers==5 && !run->realFirmwareAttempted &&
          !run->homeMenuBooted,"real boot still blocked");
    MisakiMega55Report report{};
    check(misaki_mega55_diagnostic(nullptr)==-1,"null bridge output rejected");
    check(misaki_mega55_diagnostic(&report)==0,"C bridge succeeds");
    check(report.abi_version==1,"ABI version");
    check(report.pass_51 && report.pass_52 && report.pass_53 &&
          report.pass_54 && report.pass_55,"all bridge stages");
    check(report.output_index_7==31 && report.gcn_instructions==44 &&
          report.pm4_packets==11,"bridge GPU values");
    check(report.guest_rax==42 && report.elf_segments==2 &&
          report.synthetic_nids==2,"bridge SCE values");
    check(report.firmware_blockers==5 && !report.firmware_attempted &&
          !report.home_menu_booted,"bridge boot gate");
    check(report.shader_hash==run->shaderHash,"C/C++ hash equality");
    std::cout<<"PASS: "<<checks<<" SCE55 checks\n";
}
