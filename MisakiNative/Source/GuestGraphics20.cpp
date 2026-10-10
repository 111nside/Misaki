#include "../Include/GuestGraphics20.hpp"

#include <limits>
#include <utility>
#include <vector>

namespace misaki {
namespace {
constexpr std::uint64_t guestCode = 0x2000;
constexpr std::uint32_t maxCommands = 32;
constexpr std::uint64_t xyMask = 0xffffffffULL;

std::uint64_t pair32(std::uint32_t low, std::uint32_t high) {
    return std::uint64_t(low) | (std::uint64_t(high) << 32);
}
void write64LE(std::vector<std::uint8_t> &bytes, std::uint64_t word) {
    for (unsigned k = 0; k < 8; ++k)
        bytes.push_back(static_cast<std::uint8_t>(word >> (8 * k)));
}
void movImm64(std::vector<std::uint8_t> &code, unsigned registerOpcode,
              std::uint64_t imm) {
    code.push_back(0x48);
    code.push_back(static_cast<std::uint8_t>(0xB8 + registerOpcode));
    write64LE(code, imm);
}
void emitCommand(std::vector<std::uint8_t> &bytes, G19Opcode op,
                 std::uint32_t x, std::uint32_t y, std::uint32_t w,
                 std::uint32_t h, std::uint32_t rgba, std::uint32_t texture) {
    write64LE(bytes, static_cast<std::uint32_t>(op));
    write64LE(bytes, pair32(x, y));
    write64LE(bytes, pair32(w, h));
    write64LE(bytes, pair32(rgba, texture));
}
std::vector<std::uint8_t> initialCommands() {
    std::vector<std::uint8_t> bytes;
    emitCommand(bytes, G19Opcode::clear, 0, 0, 0, 0, 0x0C1426FF, 0);
    emitCommand(bytes, G19Opcode::fill, 16, 18, 288, 144, 0x263A61FF, 0);
    emitCommand(bytes, G19Opcode::fill, 24, 28, 272, 4, 0x57D3E7FF, 0);
    emitCommand(bytes, G19Opcode::fill, 30, 145, 260, 8, 0x162544FF, 0);
    emitCommand(bytes, G19Opcode::sprite, 32, 78, 48, 48, 0, 1);
    emitCommand(bytes, G19Opcode::fill, 14, 168, 292, 8, 0x354E7FFF, 0);
    emitCommand(bytes, G19Opcode::present, 0, 0, 0, 0, 0, 0);
    return bytes;
}

class GraphicsQueueService final : public IGuestServiceDispatcher {
public:
    explicit GraphicsQueueService(GuestGraphicsQueue &queue) : queue_(queue) {}
    GuestServiceAction dispatch(GuestMemory &memory, X64State &cpu,
                                std::uint32_t) override {
        if (cpu.registers[0] != g20GPUService || calls_ != 0)
            return GuestServiceAction::unsupported;
        // RDI=guest command pointer, RSI=record count, RDX=fence sequence.
        if (!queue_.submit(memory, cpu.registers[7],
                           cpu.registers[6] > maxCommands ? maxCommands + 1 :
                           static_cast<std::uint32_t>(cpu.registers[6]),
                           cpu.registers[2]))
            return GuestServiceAction::memoryFault;
        ++calls_;
        cpu.registers[0] = cpu.registers[2]; // synthetic fence acknowledgement
        return GuestServiceAction::resume;
    }
    std::uint32_t calls() const { return calls_; }
private:
    GuestGraphicsQueue &queue_;
    std::uint32_t calls_ = 0;
};
} // namespace

bool GuestGraphicsQueue::submit(const GuestMemory &memory, std::uint64_t address,
                                std::uint32_t count, std::uint64_t fence) {
    if (count < 2 || count > maxCommands || queue_.size() >= 2 ||
        fence == 0 || fence <= lastSubmittedFence_ ||
        address > std::numeric_limits<std::uint64_t>::max() -
                  (std::uint64_t(count) * g20RecordBytes - 1)) return false;
    G19Scene scene;
    scene.spriteTexture = makeG19SpriteTexture();
    scene.commands.reserve(count);
    for (std::uint32_t i = 0; i < count; ++i) {
        const auto at = address + std::uint64_t(i) * g20RecordBytes;
        const auto rawKind = memory.read64(at);
        const auto xy = memory.read64(at + 8);
        const auto wh = memory.read64(at + 16);
        const auto colorAndTexture = memory.read64(at + 24);
        if (!rawKind || !xy || !wh || !colorAndTexture || *rawKind < 1 || *rawKind > 4)
            return false;
        scene.commands.push_back({
            static_cast<G19Opcode>(*rawKind),
            static_cast<std::uint32_t>(*xy & xyMask),
            static_cast<std::uint32_t>(*xy >> 32),
            static_cast<std::uint32_t>(*wh & xyMask),
            static_cast<std::uint32_t>(*wh >> 32),
            static_cast<std::uint32_t>(*colorAndTexture & xyMask),
            static_cast<std::uint32_t>(*colorAndTexture >> 32)
        });
    }
    // Validation AND drawing are completed before committing the fence.
    auto surface = rasterizeG19(scene);
    if (!surface) return false;
    queue_.push_back({fence, std::move(scene), std::move(*surface)});
    lastSubmittedFence_ = fence;
    return true;
}

std::optional<G20Submission> GuestGraphicsQueue::take() {
    if (queue_.empty()) return std::nullopt;
    G20Submission result = std::move(queue_.front());
    queue_.pop_front();
    completedFence_ = result.fence; // synchronous software framebuffer completed
    return result;
}

std::optional<G20Diagnostic> runG20GuestFrame(std::uint32_t frameIndex) {
    const std::uint32_t spriteX = 32 + 2 * (frameIndex % 90);
    const std::uint64_t fence = std::uint64_t(frameIndex) + 1;
    const auto commandBytes = initialCommands();
    std::vector<std::uint8_t> code;
    // MOV RDI, queue.sprite.xy; MOV RAX, packed(x,y); MOV [RDI],RAX
    movImm64(code, 7, g20GuestQueueAddress + 4 * g20RecordBytes + 8);
    movImm64(code, 0, pair32(spriteX, 78));
    code.insert(code.end(), {0x48, 0x89, 0x07});
    // Guest owns the packet array and submits its address/count/fence.
    movImm64(code, 7, g20GuestQueueAddress);
    movImm64(code, 6, 7);
    movImm64(code, 2, fence);
    movImm64(code, 0, g20GPUService);
    code.insert(code.end(), {0x0F, 0x05, 0xF4}); // SYSCALL; HLT

    GuestMemory memory;
    if (!memory.map(guestCode, code.size(), permission::read | permission::write) ||
        !memory.writeBytes(guestCode, code.data(), code.size()) ||
        !memory.protect(guestCode, code.size(), permission::read | permission::execute) ||
        !memory.map(g20GuestQueueAddress, commandBytes.size(),
                    permission::read | permission::write) ||
        !memory.writeBytes(g20GuestQueueAddress, commandBytes.data(), commandBytes.size()))
        return std::nullopt;

    GuestGraphicsQueue queue;
    GraphicsQueueService service(queue);
    PortableX64Backend backend(std::move(memory), guestCode, &service);
    const auto execution = backend.run(32);
    if (execution.stop != X64Stop::halted ||
        execution.state.instructions != 9 || !execution.stackRestored() ||
        execution.rax() != fence || service.calls() != 1 ||
        queue.pending() != 1) return std::nullopt;

    auto submission = queue.take();
    if (!submission || queue.pending() != 0 || queue.completedFence() != fence ||
        submission->surface.filledRects != 4 ||
        submission->surface.texturedSprites != 1 ||
        submission->scene.commands.size() != 7 ||
        submission->scene.commands[4].x != spriteX ||
        submission->scene.commands[4].y != 78) return std::nullopt;

    G20Diagnostic report;
    report.submitted = std::move(*submission);
    report.execution = execution;
    report.serviceCalls = service.calls();
    report.commandCount = 7;
    report.spriteX = spriteX;
    report.spriteY = 78;
    report.queueDrained = queue.pending() == 0;
    report.completedFence = queue.completedFence();
    return report;
}
} // namespace misaki
