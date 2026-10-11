#include "GuestBootCoordinator.hpp"
#include <cstdlib>
#include <iostream>
namespace{int n=0;void yes(bool v,const char*s){++n;if(!v){std::cerr<<"FAIL39: "<<s<<'\n';std::exit(1);}}}
int main(){using namespace misaki;
 auto d=runEarlyBootDiagnostic();yes(bool(d),"early boot diagnostic assembled");
 yes(d->events.size()==5,"five state events");
 for(int i=0;i<5;++i)yes(static_cast<int>(d->events[i].phase)==36+i,"ordered phase number");
 for(int i=0;i<4;++i)yes(d->events[i].passed,"synthetic phase passed");
 yes(!d->events[4].passed,"real firmware explicitly blocked");
 yes(d->executable.kind==PS4ContainerKind::self && d->executable.encryptedSegments==1,"SELF metadata recognized");
 yes(!d->executable.readyToExecute,"SELF not launched");
 yes(d->resolvedImports==2,"guest synthetic imports");
 yes(d->shaderLanes==4 && d->shaderOperations==44,"four GCN lanes integrated");
 yes(d->sessionProcesses==2 && d->sessionInstructions>0,"two integrated guest processes");
 yes(d->frameSprites==2 && d->pm4Packets==11,"GPU framebuffer and PM4 state");
 yes(d->lastFence==1,"fence completion from integrated guest");
 yes(d->syntheticDiagnosticsPassed,"bounded integration healthy");
 yes(!d->firmwareLaunched,"no real firmware boot claim");
 auto again=runEarlyBootDiagnostic();yes(again && again->sessionInstructions==d->sessionInstructions,"deterministic integration");
 std::cout<<"PASS: "<<n<<" boot-integration checks\n";
}
