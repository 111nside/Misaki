#include "GuestGraphics20.hpp"
#include "MisakiGPU20Bridge.h"

#include <cstdint>
#include <cstdlib>
#include <iostream>
#include <limits>
#include <vector>

namespace {
int checks = 0;
void check(bool good, const char *reason) {
    ++checks;
    if (!good) { std::cerr << "FAILED: " << reason << '\n'; std::exit(1); }
}
void append(std::vector<std::uint8_t> &v, std::uint64_t value) {
    for (unsigned i=0;i<8;++i) v.push_back(static_cast<std::uint8_t>(value >> (8*i)));
}
std::uint64_t packed(std::uint32_t lo, std::uint32_t hi) {
    return std::uint64_t(lo) | std::uint64_t(hi) << 32;
}
void command(std::vector<std::uint8_t> &v, std::uint32_t type,
             std::uint64_t xy, std::uint64_t wh, std::uint64_t rgbaTexture) {
    append(v,type);append(v,xy);append(v,wh);append(v,rgbaTexture);
}
std::vector<std::uint8_t> packet() {
    std::vector<std::uint8_t> v;
    command(v,1,0,0,packed(0x0C1426FF,0));
    command(v,2,packed(16,18),packed(288,144),packed(0x263A61FF,0));
    command(v,2,packed(24,28),packed(272,4),packed(0x57D3E7FF,0));
    command(v,2,packed(30,145),packed(260,8),packed(0x162544FF,0));
    command(v,3,packed(32,78),packed(48,48),packed(0,1));
    command(v,2,packed(14,168),packed(292,8),packed(0x354E7FFF,0));
    command(v,4,0,0,0);
    return v;
}
}

int main() {
    using namespace misaki;
    auto d0 = runG20GuestFrame(0);
    check(bool(d0), "guest CPU queue submission succeeds");
    check(d0->execution.stop == X64Stop::halted, "guest execution halted");
    check(d0->execution.state.instructions == 9, "nine real guest instructions");
    check(d0->execution.stackRestored(), "stack restored");
    check(d0->execution.rax() == 1, "fence 1 returned through guest RAX");
    check(d0->serviceCalls == 1, "one queue service call");
    check(d0->commandCount == 7, "seven guest command records");
    check(d0->spriteX == 32 && d0->spriteY == 78, "guest-written XY decoded");
    check(d0->submitted.fence == 1 && d0->completedFence == 1, "fence signaled after software render");
    check(d0->queueDrained, "queue drained");
    check(d0->submitted.surface.pixels.size() == 320*180*4, "RGBA8 framebuffer dimensions");
    check(d0->submitted.surface.filledRects == 4 && d0->submitted.surface.texturedSprites == 1,
          "four fills and one sprite");
    check(d0->submitted.surface.checksum != 0, "checksum exists");
    check(runG20GuestFrame(0)->submitted.surface.checksum == d0->submitted.surface.checksum,
          "deterministic frame");
    check(runG20GuestFrame(1)->spriteX == 34, "guest animation advances");
    check(runG20GuestFrame(89)->spriteX == 210, "sprite bounded at frame 89");
    check(runG20GuestFrame(90)->spriteX == 32, "animation wraps");
    check(runG20GuestFrame(90)->submitted.surface.checksum == d0->submitted.surface.checksum,
          "cycle repeats");
    check(runG20GuestFrame(89)->submitted.fence == 90, "fence follows frame index");

    auto raw = packet();
    GuestMemory mem;
    check(mem.map(0x5000,raw.size(),permission::read|permission::write),"guest packet mapped");
    check(mem.writeBytes(0x5000,raw.data(),raw.size()),"guest command packet loaded");
    GuestGraphicsQueue q;
    check(!q.submit(mem,0x5000,1,1),"reject underlength");
    check(!q.submit(mem,0x5000,33,1),"reject excess commands");
    check(!q.submit(mem,0x5000,7,0),"reject zero fence");
    check(!q.submit(mem,std::numeric_limits<std::uint64_t>::max()-9,7,1),"reject address overflow");
    check(!q.submit(mem,0x9000,7,1),"reject unmapped guest source");
    check(q.pending()==0 && q.lastSubmittedFence()==0,"bad submissions do not change state");
    check(q.submit(mem,0x5000,7,5),"accept valid guest queue");
    check(q.pending()==1 && q.lastSubmittedFence()==5,"enqueue and fence visible");
    check(!q.submit(mem,0x5000,7,4),"reject stale fence");
    check(!q.submit(mem,0x5000,7,5),"reject duplicate fence");
    check(q.pending()==1,"stale submissions do not affect queue");
    // Prove that queued commands do NOT borrow the guest's memory.
    check(mem.write64(0x5000 + 4*32 + 8, packed(60,78)),"mutate guest queue after submission");
    check(q.submit(mem,0x5000,7,6),"second submission queued");
    check(!q.submit(mem,0x5000,7,7),"two-buffer queue cap");
    auto a=q.take();
    check(bool(a) && a->fence==5 && a->scene.commands[4].x==32,"snapshot preserves original guest packet");
    check(q.completedFence()==5,"first fence signals in submission order");
    auto b=q.take();
    check(bool(b) && b->fence==6 && b->scene.commands[4].x==60,"second guest packet sees modified XY");
    check(q.completedFence()==6 && q.pending()==0,"second fence and empty queue");
    check(!q.take(),"empty queue has no frame");
    check(a->surface.checksum!=b->surface.checksum,"different queue frames rasterize differently");
    check(q.submit(mem,0x5000,7,7),"queue can accept next frame after drain");

    // Input protection: a readable virtual address is required.
    check(mem.protect(0x5000,raw.size(),permission::write),"make packet write-only");
    check(!q.submit(mem,0x5000,7,8),"reject unreadable guest packet");
    check(q.pending()==1 && q.lastSubmittedFence()==7,"protected failure transactional");
    check(mem.protect(0x5000,raw.size(),permission::read|permission::write),"restore read rights");
    check(mem.write64(0x5000,99),"write unknown command kind");
    check(!q.submit(mem,0x5000,7,8),"reject unknown opcode");
    check(q.lastSubmittedFence()==7,"bad opcode does not advance fence");
    check(mem.write64(0x5000,1),"restore opcode");
    check(mem.write64(0x5000+4*32+8,packed(319,78)),"set out-of-bounds sprite");
    check(!q.submit(mem,0x5000,7,8),"reject out-of-bounds sprite");
    check(mem.write64(0x5000+4*32+8,packed(60,78)),"restore sprite XY");
    check(mem.write64(0x5000+6*32,2),"replace final present with fill");
    check(!q.submit(mem,0x5000,7,8),"reject missing present");
    check(mem.write64(0x5000+6*32,4),"restore final present");
    check(mem.write64(0x5000+2*32+24,packed(0x57D3E700,0)),"write nonopaque fill");
    check(!q.submit(mem,0x5000,7,8),"reject invalid fill alpha");
    check(q.lastSubmittedFence()==7 && q.pending()==1,"all corruptions isolated");

    // Stable C ABI bridge. A too-small output must not change a single byte.
    std::vector<std::uint8_t> framebuffer(320*180*4,0xA7);
    MisakiGPU20Report report{};
    check(misaki_gpu20_render(0,framebuffer.data(),framebuffer.size(),&report)==0,"C framebuffer export succeeds");
    check(report.abi_version==1 && report.width==320 && report.height==180,"C metadata");
    check(report.bytes_written==framebuffer.size() && report.frame_index==0,"C frame and count");
    check(report.command_count==7 && report.rectangles==4 && report.sprites==1,"C render counts");
    check(report.guest_instructions==9 && report.guest_service_calls==1,"C CPU service counts");
    check(report.guest_halted && report.stack_restored && report.queue_drained,"C guest lifecycle");
    check(report.sprite_x==32 && report.sprite_y==78,"C sprite XY");
    check(report.submitted_fence==1 && report.completed_fence==1,"C fence completion");
    check(report.checksum==d0->submitted.surface.checksum,"C checksum equals native");
    check(framebuffer==d0->submitted.surface.pixels,"C framebuffer equals native");
    std::fill(framebuffer.begin(),framebuffer.end(),0xA7);
    check(misaki_gpu20_render(0,framebuffer.data(),framebuffer.size()-1,&report)==-3,
          "short C buffer rejected");
    check(framebuffer.front()==0xA7 && framebuffer.back()==0xA7,"no partial framebuffer on error");
    check(report.bytes_written==0,"C report zeroed on error");
    check(misaki_gpu20_render(0,nullptr,framebuffer.size(),&report)==-1,"null framebuffer rejected");
    check(misaki_gpu20_render(0,framebuffer.data(),framebuffer.size(),nullptr)==-1,"null report rejected");
    std::cout<<"PASS: "<<checks<<" native command queue assertions\n";
}
