#include "GuestIntegratedRuntime.hpp"
#include <cstdlib>
#include <iostream>
using namespace misaki;
static unsigned checks=0;
static void check(bool ok,const char *why){ ++checks;if(!ok){std::cerr<<"FAIL: "<<why<<'\n';std::exit(1);} }
static std::uint64_t invoke(IntegratedProcessServices &s,GuestMemory &m,
      std::uint64_t op,std::uint64_t addr,std::uint64_t count,
      std::uint64_t input=0,std::uint64_t output=0){
    X64State st{};st.registers[0]=op;st.registers[7]=addr;
    st.registers[6]=count;st.registers[2]=input;st.registers[1]=output;
    check(s.dispatch(m,st,1001)==GuestServiceAction::resume,"contained GCN service");
    return st.registers[0];
}
static std::uint32_t u32(const GuestMemory &memory,std::uint64_t base){
    std::uint32_t v=0;for(int i=0;i<4;++i){auto b=memory.read8(base+i);check(bool(b),"output readable");v|=std::uint32_t(*b)<<(i*8);}return v;
}
int main(){
    GuestVirtualFileSystem fs;IntegratedProcessServices service(fs,1001);GuestMemory mem;
    check(installRuntimeGCN(mem),"install shader/descriptor/inputs with permissions");
    check(installRuntimePM4(mem),"install nested PM4 streams");
    check(invoke(service,mem,runtime_service::pm4Inspect,0x40000,
                 makePM4IndirectFixture().rootWords.size())==11,"trace nested PM4 packets");
    check(service.lastPM4() && service.lastPM4()->indirectBuffers==2,
          "nested PM4 register tracing");
    check(invoke(service,mem,runtime_service::shaderRun,0x100000,13,0x110000,0x110010)==31,
          "execute GCN ALU and buffer memory in CPU reference");
    check(service.shaderCalls()==1 && service.pm4Calls()==1,"distinct service paths");
    check(service.lastShader() && service.lastShader()->executed==11,"unified GCN shader ran");
    check(service.lastShader()->registers[6]==31 && service.lastShader()->registers[7]==31,
          "loaded word moved through vector register");
    check(u32(mem,0x500000+7*4)==31,"output record 7 changed");
    check(u32(mem,0x500000+6*4)==0xB0000006u,"neighbor unaffected");
    check(invoke(service,mem,runtime_service::shaderRun,0x100000,65,0x110000,0x110010)
          ==runtime_service::failure,"reject oversized shader body");
    check(mem.protect(0x500000,256,permission::read),"make output read only");
    check(invoke(service,mem,runtime_service::shaderRun,0x100000,13,0x110000,0x110010)
          ==runtime_service::failure,"reject write-protected GCN target");
    check(u32(mem,0x500000+7*4)==31,"rejected shader preserves output");
    check(mem.protect(0x40000,makePM4IndirectFixture().rootWords.size()*4,permission::write),
          "remove PM4 read permission");
    check(invoke(service,mem,runtime_service::pm4Inspect,0x40000,
                 makePM4IndirectFixture().rootWords.size())==runtime_service::failure,
          "reject unreadable PM4");
    check(service.pm4Calls()==1 && service.shaderCalls()==1,"failed requests not counted as completed");
    std::cout<<"PASS: "<<checks<<" GCN/PM4 service 33 assertions\n";
}
