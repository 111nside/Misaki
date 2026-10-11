#include "GuestPersistent59.hpp"
#include <cstdlib>
#include <iostream>
using namespace misaki;
static unsigned checks=0;
static void ok(bool x,const char*s){++checks;if(!x){std::cerr<<"FAIL 59: "<<s<<'\n';std::exit(1);}}
int main(){
 PersistentGuest59 runtime;
 const auto program=makePersistent59Program();
 ok(!runtime.add(0,program),"zero PID rejected");
 ok(runtime.add(201,program),"first guest process");
 ok(runtime.add(202,program),"second guest process");
 ok(!runtime.add(201,program),"duplicate PID rejected");
 ok(!runtime.step(0)&&!runtime.step(33),"unbounded quanta rejected");
 ok(runtime.step(1),"advance one step");
 ok(runtime.instructions()==2,"incremental execution persists");
 ok(!runtime.allExited(),"not erroneously marked exited");
 for(int i=0;i<16&&!runtime.allExited();++i)ok(runtime.step(2),"step guest process");
 ok(runtime.allExited(),"both guests exit");
 ok(runtime.exited()==2&&runtime.yields()==2,"yield/exit properly handled");
 ok(runtime.serviceCalls()==8&&runtime.instructions()==16,"all instructions completed");
 ok(runtime.contextSwitches()>=8,"scheduler context switches");
 ok(runtime.step(2),"step after both exited is safe");
 auto report=runPersistent59Diagnostic();
 ok(bool(report),"persistent diagnostic");
 ok(report->exited==2&&report->calls==8&&report->yields==2,"diagnostic counters");
 std::cout<<"PASS "<<checks<<" M59 checks\n";
}
