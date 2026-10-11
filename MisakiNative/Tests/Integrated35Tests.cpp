#include "GuestIntegratedRuntime.hpp"
#include "MisakiIntegratedRuntimeBridge.h"
#include <algorithm>
#include <cstdlib>
#include <iostream>
#include <vector>
using namespace misaki;
static unsigned checks=0;
static void check(bool ok,const char *why){++checks;if(!ok){std::cerr<<"FAIL: "<<why<<'\n';std::exit(1);} }
int main(){
    auto run=runIntegratedMegaDiagnostic();
    check(bool(run),"integration fixture runs");
    check(run->stage31,"31 CPU/process scheduling");
    check(run->stage32,"32 GPU submission and completion fence");
    check(run->stage33,"33 PM4/GCN shader services");
    check(run->stage34,"34 dynamic ELF and module initialization");
    check(run->stage35,"35 guest VFS and service router");
    check(run->runtime.processes.size()==2,"two guest processes");
    const auto &app=run->runtime.processes[0];
    check(app.lastRead==std::vector<std::uint8_t>({'H','e','l','l','o'}),
          "VFS read reaches guest process data memory");
    check(app.pm4Packets==11 && app.shaderExecuted==11,"PM4 and shader same runtime");
    check(app.pendingGPU==0 && app.submittedFence==app.completedFence,
          "GPU fence consumed by guest");
    check(app.framebuffer.size()==320*180*4,"complete RGBA guest image");
    check(app.framebufferSprites==2,"draw guest sprites");
    check(app.rejectedCalls==0,"all guest service submissions valid");
    MisakiIntegratedRuntimeReport c{};
    std::vector<std::uint8_t> pixels(320*180*4,0xCC);
    check(misaki_integrated_runtime_demo(nullptr,pixels.size(),&c)==-1,
          "reject null C buffer");
    check(misaki_integrated_runtime_demo(pixels.data(),pixels.size(),nullptr)==-1,
          "reject null report");
    check(misaki_integrated_runtime_demo(pixels.data(),1,&c)==-3,
          "reject undersized frame target");
    check(pixels.front()==0xCC && c.abi_version==0,"no partial writes on capacity error");
    check(misaki_integrated_runtime_demo(pixels.data(),pixels.size(),&c)==0,
          "C ABI exports integrated runtime");
    check(c.passed_31 && c.passed_32 && c.passed_33 && c.passed_34 && c.passed_35,
          "five subsystem statuses exported");
    check(c.elf_rax==42 && c.guest_pid==1001,"C ABI process results");
    check(c.gpu_calls==1 && c.pm4_packets==11 && c.shader_operations==11,
          "C ABI GPU, PM4 and shader counts");
    check(c.bytes_written==pixels.size() && pixels.front()!=0xCC,
          "C ABI copies real validated framebuffer");
    check(c.framebuffer_checksum==app.framebufferChecksum,
          "C/C++ framebuffer checksums agree");
    std::cout<<"PASS: "<<checks<<" integrated service 35 assertions\n";
}
