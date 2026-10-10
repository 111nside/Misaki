#include "GuestGraphics21.hpp"
#include "MisakiGPU21Bridge.h"

#include <array>
#include <cstdlib>
#include <iostream>
#include <limits>
#include <vector>

namespace {
int assertions = 0;
void check(bool ok, const char *label) {
    ++assertions;
    if (!ok) { std::cerr << "FAIL: " << label << '\n'; std::exit(1); }
}
std::uint64_t pair32(std::uint32_t lo, std::uint32_t hi) {
    return std::uint64_t(lo) | (std::uint64_t(hi) << 32);
}
void emit(std::vector<std::uint8_t> &bytes, std::uint64_t word) {
    for (unsigned k = 0; k < 8; ++k)
        bytes.push_back(static_cast<std::uint8_t>(word >> (8 * k)));
}
void packet(std::vector<std::uint8_t> &bytes, misaki::G21Opcode opcode,
            std::uint32_t x = 0, std::uint32_t y = 0,
            std::uint32_t w = 0, std::uint32_t h = 0,
            std::uint32_t data = 0, std::uint32_t extra = 0) {
    emit(bytes, static_cast<std::uint32_t>(opcode));
    emit(bytes, pair32(x,y));
    emit(bytes, pair32(w,h));
    emit(bytes, pair32(data, extra));
}
std::vector<misaki::G21Packet> base() {
    using misaki::G21Opcode;
    return {{G21Opcode::clear,0,0,0,0,0x000000ff,0},
            {G21Opcode::state,0,0,0,0,0xffffffff,0},
            {G21Opcode::sprite,25,25,16,16,1,0},
            {G21Opcode::present,0,0,0,0,0,0}};
}
}

int main() {
    using namespace misaki;
    auto run = runG21GuestFrame(0, 0);
    check(bool(run), "main frame runs");
    check(run->guest.stop == X64Stop::halted, "CPU halted");
    check(run->guest.stackRestored(), "stack restored");
    check(run->guest.state.instructions == 15, "three guest memory patches and submit");
    check(run->guest.rax() == 1, "fence returned via guest RAX");
    check(run->serviceCalls == 1, "one guest GPU service");
    check(run->frame.packets.size() == 10, "ten GPU packets");
    check(run->frame.surface.rectangles == 2, "two fills");
    check(run->frame.surface.sprites == 2, "two sprites");
    check(run->frame.surface.distinctTextures == 2, "two textures bound");
    check(run->frame.surface.stateChanges == 2, "two render states");
    check(run->frame.surface.scissorChanges == 2, "scissor set/reset");
    check(run->frame.surface.effectMode == 0, "default shader");
    check(run->firstSpriteX == 32 && run->secondSpriteX == 208, "guest start positions");
    check(run->queueDrained && run->completedFence == 1, "fence completed");
    check(run->frame.surface.pixels.size() == 320u * 180u * 4u, "complete RGBA buffer");
    check(run->frame.surface.checksum != 0, "frame checksum");
    check(runG21GuestFrame(0,0)->frame.surface.checksum == run->frame.surface.checksum,
          "deterministic output");

    for (std::uint32_t effect = 0; effect < 4; ++effect) {
        auto r = runG21GuestFrame(20, effect);
        check(bool(r), "all shader variants run guest code");
        check(r->frame.surface.effectMode == effect, "guest-selected shader propagated");
        check(r->firstSpriteX == 72 && r->secondSpriteX == 188, "two moving sprites");
        check(r->frame.fence == 21 && r->completedFence == 21, "updated fence");
        check(r->guest.state.instructions == 15, "guest instruction count stable");
    }
    check(!runG21GuestFrame(0, 4), "unsupported shader mode rejected");
    check(runG21GuestFrame(90, 2)->firstSpriteX == 32, "animation wraps at 90");
    check(runG21GuestFrame(90, 2)->secondSpriteX == 208, "second sprite wraps");
    check(runG21GuestFrame(1, 0)->frame.surface.checksum != run->frame.surface.checksum,
          "moving sprite changes rendered frame");
    check(runG21GuestFrame(1, 0)->frame.surface.checksum !=
          runG21GuestFrame(1, 1)->frame.surface.checksum, "guest shader state changes checksum");

    auto scene = base();
    check(bool(rasterizeG21(scene)), "simple scene valid");
    const auto normal = rasterizeG21(scene);
    check(normal->sprites == 1 && normal->distinctTextures == 1, "texture 1 available");
    check(normal->pixels[(33u * 320u + 33u) * 4u] != 0,
          "texture writes non-transparent center pixel");
    scene[2].data = 2;
    auto textureTwo = rasterizeG21(scene);
    check(bool(textureTwo), "texture 2 available");
    check(textureTwo->pixels != normal->pixels, "textures differ visibly");
    auto sceneBlend = base();
    sceneBlend.insert(sceneBlend.end() - 1,
                      {G21Opcode::state,1,2,0,0,0xffffffff,0});
    sceneBlend.insert(sceneBlend.end() - 1,
                      {G21Opcode::sprite,25,25,16,16,2,0});
    auto additive = rasterizeG21(sceneBlend);
    check(bool(additive), "additive blending with texture 2 works");
    check(additive->effectMode == 2, "effect state propagated");
    check(additive->distinctTextures == 2 && additive->sprites == 2,
          "two textures on same framebuffer");
    check(additive->pixels != normal->pixels, "additive blending changes surface");
    sceneBlend[3].x = 0; // alpha blend instead
    const auto alpha = rasterizeG21(sceneBlend);
    check(bool(alpha) && alpha->pixels != additive->pixels,
          "alpha and additive blend differ");
    auto clipped = base();
    clipped.insert(clipped.begin() + 2,
                   {G21Opcode::scissor,33,33,1,1,0,0});
    auto clippedFrame = rasterizeG21(clipped);
    check(bool(clippedFrame), "one-pixel scissor valid");
    check(clippedFrame->pixels[(33u * 320u + 33u) * 4u] != 0,
          "scissor retains inside pixel");
    check(clippedFrame->pixels[(34u * 320u + 34u) * 4u] == 0,
          "scissor discards outside pixel");
    check(clippedFrame->scissorChanges == 1, "one scissor state recorded");

    auto invalid = base();
    invalid[0].data &= 0xffffff00;
    check(!rasterizeG21(invalid), "reject transparent clear");
    invalid = base(); invalid[0].opcode = G21Opcode::sprite;
    check(!rasterizeG21(invalid), "reject missing clear");
    invalid = base(); invalid[3].opcode = G21Opcode::fill;
    check(!rasterizeG21(invalid), "reject missing present");
    invalid = base(); invalid[1].x = 2;
    check(!rasterizeG21(invalid), "reject invalid blend mode");
    invalid = base(); invalid[1].y = 4;
    check(!rasterizeG21(invalid), "reject invalid shader mode");
    invalid = base(); invalid[1].extra = 1;
    check(!rasterizeG21(invalid), "reject state reserved bits");
    invalid = base(); invalid[2].data = 3;
    check(!rasterizeG21(invalid), "reject unknown texture ID");
    invalid = base(); invalid[2].width = std::numeric_limits<std::uint32_t>::max();
    check(!rasterizeG21(invalid), "reject out of bounds sprite");
    invalid = base(); invalid[3].extra = 2;
    check(!rasterizeG21(invalid), "reject invalid present payload");
    invalid = base(); invalid.insert(invalid.begin()+2,
                                    {G21Opcode::scissor,0,0,0,1,0,0});
    check(!rasterizeG21(invalid), "reject zero-size scissor");
    invalid = base(); invalid.insert(invalid.begin()+2,
                                    {G21Opcode::scissor,319,0,2,1,0,0});
    check(!rasterizeG21(invalid), "reject out-of-bounds scissor");
    invalid = base(); invalid[2].extra = 1;
    check(!rasterizeG21(invalid), "reject extra sprite metadata");
    invalid = base(); invalid.push_back({G21Opcode::present,0,0,0,0,0,0});
    check(!rasterizeG21(invalid), "reject second present");
    invalid = base(); invalid[2].opcode = static_cast<G21Opcode>(900);
    check(!rasterizeG21(invalid), "reject invalid opcode");
    invalid = base(); invalid.resize(33, invalid.front());
    check(!rasterizeG21(invalid), "cap packet count");

    GuestMemory mem;
    std::vector<std::uint8_t> bytes;
    packet(bytes, G21Opcode::clear,0,0,0,0,0x0c1426ff);
    packet(bytes, G21Opcode::state,0,0,0,0,0xffffffff);
    packet(bytes, G21Opcode::sprite,20,20,32,32,1);
    packet(bytes, G21Opcode::present);
    check(mem.map(0x6000, bytes.size(), permission::read | permission::write),
          "map queue guest memory");
    check(mem.writeBytes(0x6000, bytes.data(), bytes.size()), "write queue packets");
    GuestGraphics21Queue queue;
    check(!queue.submit(mem,0x6000,0,1), "reject empty queue");
    check(!queue.submit(mem,0x6000,33,1), "reject oversized queue");
    check(!queue.submit(mem,0x6000,4,0), "reject zero fence");
    check(!queue.submit(mem,0x6001,4,1), "reject unaligned input packets");
    check(!queue.submit(mem,0x7000,4,1), "reject unallocated memory");
    check(!queue.submit(mem,std::numeric_limits<std::uint64_t>::max()-2,4,1),
          "reject guest pointer overflow");
    check(queue.submit(mem,0x6000,4,1), "accept valid guest packets");
    check(queue.pending() == 1 && queue.lastSubmittedFence() == 1,
          "first fence visible after validation");
    check(!queue.submit(mem,0x6000,4,1), "reject reused fence");
    check(queue.submit(mem,0x6000,4,2), "two-deep queue accepts second frame");
    check(!queue.submit(mem,0x6000,4,3), "queue bounds checked");
    check(queue.pending() == 2, "failed enqueue preserves queue depth");
    const std::uint8_t badOpcode[8] = {99,0,0,0,0,0,0,0};
    check(mem.writeBytes(0x6000, badOpcode, 8), "guest may rewrite packets");
    const auto first = queue.take();
    check(first && first->fence == 1, "FIFO first frame");
    check(first->surface.sprites == 1, "FIFO owns valid rasterized snapshot");
    check(!queue.submit(mem,0x6000,4,3), "malformed packet rejected");
    check(queue.pending() == 1 && queue.lastSubmittedFence() == 2,
          "malformed packet leaves fence untouched");
    const auto second = queue.take();
    check(second && second->fence == 2, "FIFO second frame");
    check(queue.completedFence() == 2, "completion advances on take");
    check(!queue.take(), "empty queue returns no frame");
    check(!queue.submit(mem,0x6000,4,3), "corrupt packet still rejected");
    check(mem.protect(0x6000,bytes.size(),permission::write), "remove read permission");
    check(!queue.submit(mem,0x6000,4,3), "unreadable guest memory rejected");

    std::vector<uint8_t> output(320*180*4,0x5d);
    MisakiGPU21Report report{};
    check(misaki_gpu21_render(0,0,output.data(),output.size(), &report) == 0,
          "C ABI renders native frame");
    check(report.abi_version == 1 && report.width == 320 && report.height == 180,
          "C ABI dimensions");
    check(report.bytes_written == output.size(), "C ABI output size");
    check(report.command_count == 10 && report.sprites == 2 && report.rectangles == 2,
          "C ABI packet counts");
    check(report.distinct_textures == 2 && report.state_changes == 2 &&
          report.scissor_changes == 2, "C ABI advanced state counts");
    check(report.guest_instructions == 15 && report.guest_halted && report.stack_restored,
          "C ABI guest completed");
    check(report.guest_service_calls == 1 && report.queue_drained,
          "C ABI submitted and drained queue");
    check(report.submitted_fence == 1 && report.completed_fence == 1,
          "C ABI fence matched");
    check(report.first_sprite_x == 32 && report.second_sprite_x == 208,
          "C ABI sprite positions");
    check(report.checksum == run->frame.surface.checksum, "C ABI checksum stable");
    check(output == run->frame.surface.pixels, "C ABI full framebuffer matches C++");
    output.assign(output.size(),0x5d);
    check(misaki_gpu21_render(3,1,output.data(),output.size()-1,&report) == -3,
          "C ABI capacity error");
    check(output.front() == 0x5d && output.back() == 0x5d,
          "C ABI errors never partially write output");
    check(report.bytes_written == 0, "C ABI clears report on failure");
    check(misaki_gpu21_render(0,4,output.data(),output.size(),&report) == -2,
          "C ABI rejects invalid shader mode");
    check(misaki_gpu21_render(0,0,nullptr,output.size(),&report) == -1,
          "C ABI rejects null pixels");
    check(misaki_gpu21_render(0,0,output.data(),output.size(),nullptr) == -1,
          "C ABI rejects null report");

    std::cout << "PASS: " << assertions << " native milestone 21 assertions\n";
}
