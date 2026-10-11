#include "GuestIntegratedRuntime.hpp"
#include <cstdlib>
#include <iostream>
#include <vector>
using namespace misaki;
static unsigned checks=0;
static void check(bool ok, const char *why) { ++checks; if (!ok) { std::cerr << "FAIL: " << why << '\n'; std::exit(1); } }
static std::uint64_t call(IntegratedProcessServices &service, GuestMemory &memory,
                           std::uint64_t number, std::uint64_t arg=0,
                           std::uint64_t count=0, std::uint64_t fence=0) {
    X64State cpu{};
    cpu.registers[0]=number; cpu.registers[7]=arg;
    cpu.registers[6]=count; cpu.registers[2]=fence;
    check(service.dispatch(memory,cpu,1001)==GuestServiceAction::resume, "GPU service never exposes host syscall");
    return cpu.registers[0];
}
int main() {
    GuestVirtualFileSystem fs;
    IntegratedProcessServices gpu(fs,1001);
    GuestMemory memory;
    const auto cmds=makeRuntimeG21Commands();
    check(cmds.size()==10*32, "ten GPU records");
    check(memory.map(0x6000,cmds.size(),permission::read|permission::write),"map guest commands");
    check(memory.writeBytes(0x6000,cmds.data(),cmds.size()),"write guest queue");
    check(call(gpu,memory,runtime_service::gpuPoll)==0, "initial completed fence zero");
    check(call(gpu,memory,runtime_service::gpuWait,1)==runtime_service::failure, "wait without submit rejected");
    check(call(gpu,memory,runtime_service::gpuSubmit,0x6000,10,2)==runtime_service::failure,
          "reject out of order fence");
    check(call(gpu,memory,runtime_service::gpuSubmit,0x6000,10,1)==1,"submit fence one");
    check(call(gpu,memory,runtime_service::gpuSubmit,0x6000,10,2)==2,"submit fence two");
    check(gpu.pendingGPU()==2 && gpu.completedFence()==0, "queue bounded and incomplete before wait");
    check(call(gpu,memory,runtime_service::gpuSubmit,0x6000,10,3)==runtime_service::failure,
          "third frame refused when queue full");
    check(gpu.pendingGPU()==2 && gpu.submittedFence()==2,"failure leaves queue/fence unchanged");
    check(call(gpu,memory,runtime_service::gpuWait,1)==1,"finish first frame");
    check(gpu.pendingGPU()==1 && gpu.completedFence()==1,"one pending frame");
    check(gpu.lastFrame() && gpu.lastFrame()->surface.sprites==2, "drawn frame has two sprites");
    const auto firstChecksum=gpu.lastFrame()->surface.checksum;
    check(firstChecksum!=0,"nonzero framebuffer checksum");
    check(call(gpu,memory,runtime_service::gpuWait,2)==2,"finish second frame");
    check(gpu.pendingGPU()==0 && gpu.completedFence()==2,"queue drained and fence advanced");
    check(call(gpu,memory,runtime_service::gpuPoll)==2,"poll sees completed fence");
    check(call(gpu,memory,runtime_service::gpuWait,4)==runtime_service::failure,"reject wait beyond submit");
    check(gpu.gpuCalls()==2,"only two successful GPU submits");
    check(memory.protect(0x6000,cmds.size(),permission::write),"remove read access");
    check(call(gpu,memory,runtime_service::gpuSubmit,0x6000,10,3)==runtime_service::failure,
          "unreadable command buffer rejected");
    check(gpu.submittedFence()==2,"unreadable submit leaves fence unchanged");
    check(gpu.lastFrame()->surface.checksum==firstChecksum,"completed frame remains immutable");
    std::cout<<"PASS: "<<checks<<" GPU queue and fence 32 assertions\n";
}
