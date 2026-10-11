#include "GuestCompatibility60.hpp"
#include "GuestMega60.hpp"
#include "GuestSCE56.hpp"
#include "GuestPS4Formats.hpp"
#include "MisakiMega60Bridge.h"
#include <cstdlib>
#include <iostream>
#include <string>
#include <vector>
using namespace misaki;
static unsigned checks=0;
static void ok(bool x,const char*s){++checks;if(!x){std::cerr<<"FAIL 60: "<<s<<'\n';std::exit(1);}}
int main(){
 auto encrypted=makePS4StructuralFixture(true);
 auto a=analyzeExecutable60(encrypted.data(),encrypted.size(),encrypted.size());
 ok(a.kind==Compat60Kind::self,"SELF recognized");
 ok(a.encryptedSegments==1&&a.encryptedRefused,"encrypted payload refused");
 ok(!a.canExecute&&a.findings.size()>=5,"cannot report successful firmware boot");
 auto elf=makeSCE56Fixture();
 a=analyzeExecutable60(elf.data(),elf.size(),elf.size());
 ok(a.kind==Compat60Kind::sceELF&&a.neededModules.size()==1,"parse real-format ELF dependencies");
 ok(a.neededModules[0]=="libDemo.prx","module name is exposed");
 ok(a.dynamicSegments==1&&a.loadSegments==2,"ELF segment classification");
 ok(!a.canExecute,"inspection never executes ELF");
 a=analyzeExecutable60(elf.data(),128,elf.size());
 ok(a.partial&&a.findings.size()>=2,"report prefix truncation");
 a=analyzeExecutable60(reinterpret_cast<const std::uint8_t*>("blah"),4,4);
 ok(a.kind==Compat60Kind::unknown&&!a.canExecute,"unrecognized data safely classified");
 MisakiCompatibility60Report report{};char diagnostic[4096]{};
 ok(misaki_compat60_inspect(elf.data(),elf.size(),elf.size(),&report,diagnostic,sizeof(diagnostic))==0,"C bridge analyze ELF");
 ok(report.kind==1&&report.needed_modules==1&&report.execution_refused,"C report fidelity");
 ok(std::string(diagnostic).find("libDemo.prx")!=std::string::npos,"C report includes dependency");
 diagnostic[0]='Z';report.kind=777;
 ok(misaki_compat60_inspect(elf.data(),elf.size(),elf.size(),&report,diagnostic,2)==-2,"small output fails");
 ok(diagnostic[0]=='Z'&&report.kind==777,"no partial C output on failure");
 ok(misaki_compat60_inspect(nullptr,0,0,&report,diagnostic,sizeof(diagnostic))==-1,"null input rejected");
 ok(misaki_compat60_inspect(elf.data(),elf.size(),1,&report,diagnostic,sizeof(diagnostic))==-1,"size inconsistency rejected");
 auto mega=runMega60Diagnostic();ok(bool(mega),"five stage integrated diagnostic");
 for(bool stage:mega->stages)ok(stage,"each stage PASS");
 ok(mega->unmetFirmwarePrerequisites==5&&!mega->bootAttempted&&!mega->homeMenuBooted,"real boot blocked");
 MisakiMega60Report r{};
 ok(misaki_mega60_diagnostic(&r)==0&&r.abi_version==1,"C mega bridge");
 ok(r.pass_56&&r.pass_57&&r.pass_58&&r.pass_59&&r.pass_60,"five C-ABI stages");
 ok(r.guest_exits==2&&r.gpu_copies==1&&r.elf_dependencies==1,"native module reports");
 std::cout<<"PASS "<<checks<<" M60 checks\n";
}
