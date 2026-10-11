#include "../Include/GuestGCNMemoryStage.hpp"
#include <limits>
#include <utility>
namespace misaki {
bool installGCNMemoryStageFixture(GuestMemory &memory) {
    GuestMemory scratch=memory;
    const auto f=makePM4IndirectFixture();
    const auto commands=makeRuntimeG21Commands();
    const std::uint8_t shader[] = {0xF4}; // non-Sony test byte, NOT executed by the PM4 path
    constexpr std::size_t bytes = std::size_t(g21Width)*g21Height*4;
    if (!installPM4IndirectFixture(scratch,f) ||
        !scratch.map(0x100000, sizeof(shader), permission::read|permission::write) ||
        !scratch.writeBytes(0x100000,shader,sizeof(shader)) ||
        !scratch.protect(0x100000,sizeof(shader),permission::read|permission::execute) ||
        !scratch.map(0x20000000,bytes,permission::read|permission::write) ||
        !scratch.map(g21QueueAddress,commands.size(),permission::read|permission::write) ||
        !scratch.writeBytes(g21QueueAddress,commands.data(),commands.size())) return false;
    memory=std::move(scratch);
    return true;
}
std::optional<GCNMemoryStageReport> executeGCNMemoryStage(
    GuestMemory &memory, std::uint64_t pm4Root, std::size_t pm4Words,
    std::uint64_t commandAddress, std::uint32_t commands, std::uint64_t fence,
    std::string *error) {
    if (error) error->clear();
    const auto fail=[&](const char *s) -> std::optional<GCNMemoryStageReport> {
        if (error) *error=s;
        return std::nullopt;
    };
    if (fence != 1 || commands == 0 || commands > g21MaximumCommands)
        return fail("Unsupported GPU queue fence or packet count");
    const auto state=analyzePM4DrawStates(memory,pm4Root,pm4Words,error);
    if (!state || state->draws.size()!=1 ||
        state->draws[0].status != PM4DrawStatus::metadataReady)
        return fail("PM4 draw state has no usable target/shader metadata");
    const auto &draw=state->draws[0];
    if (draw.colorAddress > UINT64_MAX - std::size_t(g21Width)*g21Height*4 ||
        !memory.isExecutable(draw.pixelShaderAddress))
        return fail("PM4 target range or shader mapping invalid");
    GuestGraphics21Queue queue;
    if (!queue.submit(memory,commandAddress,commands,fence))
        return fail("Guest 2D command stream rejected");
    auto frame=queue.take();
    if (!frame || frame->surface.sprites!=2 ||
        frame->surface.pixels.size()!=std::size_t(g21Width)*g21Height*4 ||
        queue.completedFence()!=fence || queue.pending()!=0)
        return fail("GPU staging fence or framebuffer invalid");
    // Full transactional copy: invalid guest write permissions leave original
    // RAM and its previous framebuffer unchanged.
    GuestMemory scratch=memory;
    if (!scratch.writeBytes(draw.colorAddress,frame->surface.pixels.data(),
                            frame->surface.pixels.size()))
        return fail("Guest color target missing or not writable");
    memory=std::move(scratch);
    GCNMemoryStageReport out;
    out.pm4Packets=state->packets;
    out.gpuPackets=static_cast<std::uint32_t>(frame->packets.size());
    out.sprites=frame->surface.sprites;
    out.targetAddress=draw.colorAddress;
    out.shaderAddress=draw.pixelShaderAddress;
    out.submittedFence=fence;
    out.completedFence=queue.completedFence();
    out.framebufferChecksum=frame->surface.checksum;
    out.shaderExecutable=true;
    out.targetMappedWritable=true;
    return out;
}
} // namespace misaki
