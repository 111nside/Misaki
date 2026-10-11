#include "GuestGPUMemory58.hpp"
#include <cstdlib>
#include <iostream>
#include <vector>
using namespace misaki;
static unsigned checks=0;
static void ok(bool x,const char*s){++checks;if(!x){std::cerr<<"FAIL 58: "<<s<<'\n';std::exit(1);}}
int main(){
 GuestKernel53 kernel;
 auto pid=kernel.createProcess();ok(bool(pid),"process created");
 ok(kernel.map(*pid,0x800000,4096,permission::read|permission::write),"input map");
 ok(kernel.map(*pid,0x810000,4096,permission::read|permission::write),"output map");
 std::vector<std::uint8_t> pixels(256);
 for(std::size_t i=0;i<pixels.size();++i)pixels[i]=static_cast<std::uint8_t>(i);
 ok(kernel.write(*pid,0x800000,pixels.data(),pixels.size()),"guest input initialized");
 ok(kernel.protect(*pid,0x800000,4096,permission::read),"guest input read-only");
 GuestGPUMemory58 gpu(kernel);
 ok(!gpu.submit(*pid,0x800000,0x810000,65,1,1),"oversize width");
 ok(!gpu.submit(*pid,0x800000,0x810000,0,1,1),"zero width");
 ok(!gpu.submit(*pid,0x800000,0x810000,8,8,0),"zero fence");
 ok(!gpu.submit(*pid,0x800000,0x800020,8,8,1),"overlapping buffers");
 ok(!gpu.submit(*pid+9,0x800000,0x810000,8,8,1),"unknown process");
 ok(gpu.completedFence()==0&&kernel.read(*pid,0x810000)==0,"failures transactional");
 ok(gpu.submit(*pid,0x800000,0x810000,8,8,1),"valid guest surface submitted");
 for(std::size_t i=0;i<pixels.size();++i)ok(kernel.read(*pid,0x810000+i)==pixels[i],"exact pixel copied");
 ok(gpu.completedFence()==1&&gpu.copies()==1,"fence advanced once");
 ok(!gpu.submit(*pid,0x800000,0x810000,8,8,1),"duplicate fence rejected");
 ok(!gpu.submit(*pid,0x800000,0x810000,8,8,3),"nonmonotonic fence rejected");
 ok(kernel.protect(*pid,0x810000,4096,permission::read),"seal output");
 ok(!gpu.submit(*pid,0x800000,0x810000,8,8,2),"read-only output rejected");
 auto report=runGuestGPU58Diagnostic();ok(bool(report),"integrated GPU fixture");
 ok(report->pixels==256&&report->fence==1&&report->processIsolated,"report describes isolated frame");
 std::cout<<"PASS "<<checks<<" M58 checks\n";
}
