#include "GuestSCEKernel.hpp"
#include <cstdlib>
#include <iostream>
using namespace misaki;
namespace {
unsigned count=0;
void check(bool v,const char*s){++count;if(!v){std::cerr<<"FAIL 43: "<<s<<'\n';std::exit(1);}}
}
int main(){
    auto code=makeSCEKernelGuestFixture();
    GuestMemory mem;
    const std::uint8_t text[]={'H','e','l','l','o'};
    check(mem.map(0x4000,code.size(),permission::read|permission::write),"code mapped");
    check(mem.writeBytes(0x4000,code.data(),code.size()),"code copied");
    check(mem.protect(0x4000,code.size(),permission::read|permission::execute),"code RX");
    check(mem.map(0x300000,sizeof(text),permission::read|permission::write),"text mapped");
    check(mem.writeBytes(0x300000,text,sizeof(text)),"text copied");
    check(mem.protect(0x300000,sizeof(text),permission::read),"text RO");
    SCEKernelShim services(1001,1000);
    PortableX64Backend cpu(mem,0x4000,&services,1001);
    const auto result=cpu.run(50);
    check(result.stop==X64Stop::exited,"fixture stops through test exit service");
    check(result.stackRestored(),"stack intact");
    check(services.calls()==4 && services.failures()==0,"four validated calls");
    check(services.lastPID()==1001 && services.lastTicks()==1000,"pid and clock deterministic");
    check(services.output()==std::vector<std::uint8_t>(text,text+5),"isolated output copied");
    check(services.exited() && services.exitCode()==0,"reported test exit code");
    X64State st{}; st.registers[0]=sce_shim::write; st.registers[7]=0xABCDEF;
    st.registers[6]=8;
    SCEKernelShim other(1200);
    check(other.dispatch(mem,st,1200)==GuestServiceAction::resume,"invalid address returns");
    check(st.registers[0]==sce_shim::failure && other.failures()==1,"address fault reported");
    check(other.output().empty(),"failed write has no output");
    st.registers[0]=sce_shim::write; st.registers[7]=0x300000; st.registers[6]=257;
    check(other.dispatch(mem,st,1200)==GuestServiceAction::resume,"oversized output rejected");
    check(st.registers[0]==sce_shim::failure && other.output().empty(),"no partial output on oversized write");
    st.registers[0]=sce_shim::pid;
    check(other.dispatch(mem,st,999)==GuestServiceAction::resume,"wrong pid does not escape process");
    check(st.registers[0]==sce_shim::failure,"pid isolation enforced");
    st.registers[0]=0x12345678;
    check(other.dispatch(mem,st,1200)==GuestServiceAction::resume,"unknown service returns");
    check(st.registers[0]==sce_shim::failure,"unknown service blocked");
    st.registers[0]=sce_shim::exit; st.registers[7]=999;
    check(other.dispatch(mem,st,1200)==GuestServiceAction::resume,"invalid exit code rejected");
    check(!other.exited(),"invalid exit not committed");
    st.registers[0]=sce_shim::clock;
    check(other.dispatch(mem,st,1200)==GuestServiceAction::resume,"tick service works");
    check(st.registers[0]==1000,"start ticks");
    st.registers[0]=sce_shim::clock;
    check(other.dispatch(mem,st,1200)==GuestServiceAction::resume,"tick second works");
    check(st.registers[0]==1001,"monotonic time");
    std::cout<<"PASS: "<<count<<" milestone 43 system-service assertions\n";
}
