#include "GuestBootReadiness.hpp"
#include "MisakiPS4BootBridge.h"
#include <cstdlib>
#include <iostream>
namespace{int n=0;void yes(bool v,const char*s){++n;if(!v){std::cerr<<"FAIL40: "<<s<<'\n';std::exit(1);}}}
int main(){using namespace misaki;
 const auto d=runEarlyBootDiagnostic();yes(bool(d),"diagnostic exists");
 auto state=evaluatePS4BootReadiness(*d, PS4BootCapabilities{});
 yes(state.completedSyntheticChecks==4,"four synthetic integration checks");
 yes(state.totalChecks==5,"five program phases");
 yes(state.blockers.size()==5,"five missing firmware prerequisites");
 yes(!state.prerequisitesReported,"firmware not ready");
 yes(!state.firmwareExecutionAttempted && !state.homeMenuBooted,"never claim boot");
 PS4BootCapabilities c;c.realSelfPayloadLoader=true;
 state=evaluatePS4BootReadiness(*d,c);
 yes(state.blockers.size()==4,"one prerequisite resolved hypothetically");
 c.realSceKernelServices=true;c.comprehensiveGCNWavefront=true;
 state=evaluatePS4BootReadiness(*d,c);
 yes(state.blockers.size()==2,"two dependencies remain");
 c.ps4FirmwareImagesProvided=true;c.ps4FirmwareVirtualMemory=true;
 state=evaluatePS4BootReadiness(*d,c);
 yes(state.blockers.empty() && state.prerequisitesReported,"all declared prerequisites gate clear");
 yes(!state.firmwareExecutionAttempted && !state.homeMenuBooted,"no execution despite hypothetical readiness");
 auto corrupt=*d;corrupt.events[1].passed=false;
 state=evaluatePS4BootReadiness(corrupt,c);
 yes(!state.prerequisitesReported && state.blockers.size()==1,"synthetic integration failure blocks");
 corrupt=*d;corrupt.syntheticDiagnosticsPassed=false;
 yes(!evaluatePS4BootReadiness(corrupt,c).prerequisitesReported,"failed native self test blocks");
 const auto demo=runPS4BootReadinessDiagnostic();
 yes(demo && demo->blockers.size()==5 && !demo->homeMenuBooted,"default readiness diagnostic strictly blocked");
 MisakiPS4BootReport bridge{};
 yes(misaki_ps4_boot_readiness_diagnostic(nullptr)==-1,"null C report rejected");
 yes(misaki_ps4_boot_readiness_diagnostic(&bridge)==0,"C report produced");
 yes(bridge.abi_version==1 && bridge.passed_36==1 && bridge.passed_37==1 &&
     bridge.passed_38==1 && bridge.passed_39==1 && bridge.passed_40==1,"all diagnostic stages reported");
 yes(bridge.encrypted_segments==1 && bridge.sce_elf_type==0xFE10,"encrypted SELF recognized, not decrypted");
 yes(bridge.resolved_synthetic_imports==2 && bridge.cooperative_gcn_lanes==4,
     "C report import and lane counters");
 yes(bridge.readiness_blockers==5 && bridge.firmware_execution_attempted==0 &&
     bridge.home_menu_booted==0,"C report never claims real boot");
 std::cout<<"PASS: "<<n<<" firmware-gate checks\n";
}
