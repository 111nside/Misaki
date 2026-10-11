#include "GuestGCNMemoryStage.hpp"
#include <cstdlib>
#include <iostream>
using namespace misaki;
namespace {
unsigned count=0;
void check(bool v,const char*s){++count;if(!v){std::cerr<<"FAIL 44: "<<s<<'\n';std::exit(1);}}
}
int main(){
    GuestMemory memory;
    check(installGCNMemoryStageFixture(memory),"install guest PM4 target, shader, 2D commands");
    auto fixture=makePM4IndirectFixture();
    auto good=executeGCNMemoryStage(memory,fixture.root,fixture.rootWords.size(),
                                    g21QueueAddress,10,1);
    check(bool(good),"host software framebuffer staged into guest target");
    check(good->pm4Packets==11 && good->gpuPackets==10,"PM4+G21 packet counts");
    check(good->sprites==2 && good->targetAddress==0x20000000,"two sprites at target");
    check(good->shaderAddress==0x100000 && good->shaderExecutable,"shader address checked");
    check(good->submittedFence==1 && good->completedFence==1,"ordered fence");
    check(good->framebufferChecksum!=0 && good->targetMappedWritable,"committed image");
    check(memory.read8(0x20000000)==0x0c,"first pixel has background R channel");
    auto before=memory.read64(0x20000000);
    check(!executeGCNMemoryStage(memory,fixture.root,fixture.rootWords.size(),
                                 g21QueueAddress,10,0),"bad fence rejected");
    check(memory.read64(0x20000000)==before,"failed submission preserves image");
    GuestMemory shortTarget;
    check(installPM4IndirectFixture(shortTarget,fixture),"root PM4 installed independently");
    check(!executeGCNMemoryStage(shortTarget,fixture.root,fixture.rootWords.size(),
                                 g21QueueAddress,10,1),"missing guest shader rejected");
    GuestMemory protectedTarget;
    check(installGCNMemoryStageFixture(protectedTarget),"new complete fixture");
    check(protectedTarget.protect(0x20000000,std::size_t(g21Width)*g21Height*4,permission::read),"target RO");
    check(!executeGCNMemoryStage(protectedTarget,fixture.root,fixture.rootWords.size(),
                                 g21QueueAddress,10,1),"RO target rejected");
    check(protectedTarget.read8(0x20000000)==0,"target unchanged on denial");
    GuestMemory invalidShader;
    check(installGCNMemoryStageFixture(invalidShader),"new complete fixture 2");
    check(invalidShader.protect(0x100000,1,permission::read),"shader loses X");
    check(!executeGCNMemoryStage(invalidShader,fixture.root,fixture.rootWords.size(),
                                 g21QueueAddress,10,1),"shader X permission required");
    check(invalidShader.read8(0x20000000)==0,"shader failure prevents writes");
    GuestMemory malformed;
    check(installGCNMemoryStageFixture(malformed),"new complete fixture 3");
    check(malformed.write64(g21QueueAddress,999),"malform first 2D opcode");
    check(!executeGCNMemoryStage(malformed,fixture.root,fixture.rootWords.size(),
                                 g21QueueAddress,10,1),"invalid 2D queue rejected");
    check(malformed.read8(0x20000000)==0,"invalid packet no partial target write");
    check(!executeGCNMemoryStage(malformed,0x123456,5,g21QueueAddress,10,1),"unmapped PM4 rejected");
    std::cout<<"PASS: "<<count<<" milestone 44 guest GPU memory assertions\n";
}
