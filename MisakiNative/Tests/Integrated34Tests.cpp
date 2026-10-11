#include "GuestIntegratedRuntime.hpp"
#include <cstdlib>
#include <iostream>
using namespace misaki;
static unsigned checks=0;
static void check(bool ok,const char *why){++checks;if(!ok){std::cerr<<"FAIL: "<<why<<'\n';std::exit(1);} }
int main(){
    const auto elf=makeDynamicCPUFixture();const auto modules=makeRuntime34Modules();
    check(elf.size()>512 && modules.size()==2,"authored dynamic fixture and two module definitions");
    IntegratedGuestRuntime runtime;
    check(runtime.addDynamicProcess(elf,modules,"libMisakiDynamic",0x4000),
          "ET_DYN linked through live module catalog");
    check(runtime.processCount()==1,"load creates one process");
    const auto result=runtime.run(3,96);
    check(result.completed && result.processes.size()==1,"ELF guest completes in scheduler");
    const auto &p=result.processes.front();
    check(p.loadedModules==2 && p.resolvedImports==1,"two dependency modules and one dynamic import");
    check(p.execution.rax()==42,"imported function executed");
    check(p.execution.stop==X64Stop::halted && p.execution.stackRestored(),"ELF guest stopped safely");
    IntegratedGuestRuntime invalid;
    auto truncated=elf;truncated.resize(16);
    check(!invalid.addDynamicProcess(truncated,modules,"libMisakiDynamic",0x4000),
          "reject malformed guest executable");
    check(invalid.processCount()==0,"failed ELF load has no process side effects");
    check(!invalid.addDynamicProcess(elf,modules,"missing",0x4000),"reject missing lifecycle root");
    auto noExport=modules;noExport[1].exports.clear();
    check(!invalid.addDynamicProcess(elf,noExport,"libMisakiDynamic",0x4000),
          "reject unresolved ELF import");
    auto cyclic=modules;cyclic[0].needed={"libMisakiDynamic"};
    check(!invalid.addDynamicProcess(elf,cyclic,"libMisakiDynamic",0x4000),
          "reject cyclic dependencies");
    auto overlapping=modules;overlapping[0].address=0x9000;
    check(!invalid.addDynamicProcess(elf,overlapping,"libMisakiDynamic",0x4000),
          "reject overlapping RX modules");
    check(invalid.processCount()==0,"failed transitions remain transactional");
    std::cout<<"PASS: "<<checks<<" ELF/module integration 34 assertions\n";
}
