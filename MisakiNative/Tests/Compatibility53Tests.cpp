#include "GuestKernel53.hpp"
#include <iostream>
#include <cstdlib>
#include <array>
using namespace misaki;
static int checks=0;
static void check(bool ok,const char *why){++checks;if(!ok){std::cerr<<"FAIL M53: "<<why<<'\n';std::exit(1);}}
int main(){
    GuestKernel53 kernel;
    auto parent=kernel.createProcess();
    check(parent && *parent==100,"first process ID");
    check(kernel.processCount()==1,"process count");
    check(!kernel.map(*parent,0x1001,4096,permission::read),"unaligned mapping refused");
    check(!kernel.map(*parent,0x4000,4096,permission::write|permission::execute|permission::read),"W+X refused");
    check(kernel.map(*parent,0x4000,4096,permission::read|permission::write),"mapped page");
    check(!kernel.map(*parent,0x4000,4096,permission::read),"overlap refused");
    check(kernel.pages(*parent)==1,"one mapped page");
    const std::array<std::uint8_t,3> a{{3,5,7}},b{{11,13,17}};
    check(kernel.write(*parent,0x4000,a.data(),a.size()),"parent write");
    auto child=kernel.cloneProcess(*parent);
    check(child && *child==101,"cloned independent process");
    check(kernel.processCount()==2 && kernel.pages(*child)==1,"cloned state");
    check(kernel.write(*child,0x4000,b.data(),b.size()),"child changes its copy");
    check(kernel.read(*parent,0x4000)==3 && kernel.read(*child,0x4000)==11,"independent contents");
    check(kernel.protect(*child,0x4000,4096,permission::read),"sealed child page");
    check(!kernel.write(*child,0x4000,a.data(),a.size()),"write denied to child");
    check(kernel.write(*parent,0x4000,a.data(),a.size()),"parent retains write access");
    check(!kernel.unmap(*parent,0x4000,8192),"wrong region size refused");
    check(kernel.unmap(*child,0x4000,4096),"unmapped child region");
    check(!kernel.read(*child,0x4000),"unmapped memory unavailable");
    check(kernel.read(*parent,0x4000)==3,"parent not affected by unmap");
    check(kernel.terminate(*child),"terminated child");
    check(!kernel.terminate(*child),"terminated handle not reusable");
    check(!kernel.map(*child,0x9000,4096,permission::read),"stale handle refused");
    check(!kernel.cloneProcess(*child),"clone from dead handle refused");
    check(kernel.processCount()==1 && kernel.pages(999)==0,"active-process counts");
    for(unsigned i=0;i<7;++i) check(bool(kernel.createProcess()),"process budget");
    check(!kernel.createProcess(),"ninth process refused");
    auto fixture=runGuestKernel53Diagnostic();
    check(fixture && fixture->isolated && fixture->sealedReadOnly &&
          fixture->terminated && fixture->mappedPages==2 &&
          fixture->activeProcesses==1,"complete process isolation diagnostic");
    std::cout<<"PASS: "<<checks<<" process53 checks\n";
}
