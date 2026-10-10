#include "../Include/GuestGraphics21.hpp"

#include <algorithm>
#include <array>
#include <limits>
#include <utility>

namespace misaki {
namespace {
constexpr std::uint64_t guestCode = 0x2000;
constexpr std::uint64_t mask32 = 0xffffffffULL;
constexpr std::uint64_t fnvOffset = 14695981039346656037ULL;
constexpr std::uint64_t fnvPrime = 1099511628211ULL;

struct Texture {
    std::array<std::uint8_t, 16 * 16 * 4> rgba{};
};

std::array<std::uint8_t, 4> color(std::uint32_t v) {
    return {{static_cast<std::uint8_t>(v >> 24), static_cast<std::uint8_t>(v >> 16),
             static_cast<std::uint8_t>(v >> 8), static_cast<std::uint8_t>(v)}};
}

Texture texture1() {
    Texture t;
    const auto from19 = makeG19SpriteTexture();
    if (from19.rgba.size() == t.rgba.size())
        std::copy(from19.rgba.begin(), from19.rgba.end(), t.rgba.begin());
    return t;
}
Texture texture2() {
    Texture t;
    for (unsigned y = 0; y < 16; ++y) {
        for (unsigned x = 0; x < 16; ++x) {
            const bool edge = x == 0 || y == 0 || x == 15 || y == 15;
            const bool diagonal = (x / 3 + y / 3) % 2 == 0;
            const auto c = color(edge ? 0xFFFFFFFFu :
                                 diagonal ? 0xFF965CCC : 0x43E5B6AA);
            const auto offset = (y * 16 + x) * 4;
            for (unsigned k = 0; k < 4; ++k) t.rgba[offset + k] = c[k];
        }
    }
    return t;
}

bool fits(const G21Packet &p) {
    return p.width != 0 && p.height != 0 && p.x < g21Width && p.y < g21Height &&
           p.width <= g21Width - p.x && p.height <= g21Height - p.y;
}
void digest(std::uint64_t &hash, std::uint8_t v) { hash = (hash ^ v) * fnvPrime; }
void digest32(std::uint64_t &hash, std::uint32_t v) {
    for (unsigned k = 0; k < 4; ++k) digest(hash, static_cast<std::uint8_t>(v >> (k * 8)));
}
std::uint64_t pair32(std::uint32_t a, std::uint32_t b) {
    return std::uint64_t(a) | (std::uint64_t(b) << 32);
}
void write64(std::vector<std::uint8_t> &bytes, std::uint64_t v) {
    for (unsigned k = 0; k < 8; ++k) bytes.push_back(static_cast<std::uint8_t>(v >> (8 * k)));
}
void emit(std::vector<std::uint8_t> &bytes, G21Opcode op,
          std::uint32_t x = 0, std::uint32_t y = 0,
          std::uint32_t w = 0, std::uint32_t h = 0,
          std::uint32_t data = 0, std::uint32_t extra = 0) {
    write64(bytes, static_cast<std::uint32_t>(op));
    write64(bytes, pair32(x, y));
    write64(bytes, pair32(w, h));
    write64(bytes, pair32(data, extra));
}
std::vector<std::uint8_t> demoCommands() {
    std::vector<std::uint8_t> bytes;
    emit(bytes, G21Opcode::clear, 0, 0, 0, 0, 0x0C1426FFu);
    emit(bytes, G21Opcode::fill, 16, 18, 288, 144, 0x263A61FFu);
    emit(bytes, G21Opcode::scissor, 24, 28, 272, 124);
    emit(bytes, G21Opcode::state, 0, 0, 0, 0, 0xFFFFFFFFu);
    emit(bytes, G21Opcode::sprite, 32, 62, 48, 48, 1);
    emit(bytes, G21Opcode::state, 1, 0, 0, 0, 0xDAE9FFFFu);
    emit(bytes, G21Opcode::sprite, 208, 78, 56, 56, 2);
    emit(bytes, G21Opcode::scissor, 0, 0, 320, 180);
    emit(bytes, G21Opcode::fill, 14, 168, 292, 8, 0x354E7FFFu);
    emit(bytes, G21Opcode::present);
    return bytes;
}
void movImm(std::vector<std::uint8_t> &code, unsigned reg, std::uint64_t value) {
    code.push_back(0x48);
    code.push_back(static_cast<std::uint8_t>(0xb8 + reg));
    write64(code, value);
}
void patchXY(std::vector<std::uint8_t> &code, std::uint32_t commandIndex,
             std::uint32_t x, std::uint32_t y) {
    movImm(code, 7, g21QueueAddress + commandIndex * g21RecordBytes + 8);
    movImm(code, 0, pair32(x, y));
    code.insert(code.end(), {0x48, 0x89, 0x07}); // MOV [RDI],RAX
}
class G21Service final : public IGuestServiceDispatcher {
public:
    explicit G21Service(GuestGraphics21Queue &queue) : queue_(queue) {}
    GuestServiceAction dispatch(GuestMemory &memory, X64State &cpu,
                                std::uint32_t) override {
        if (calls_ != 0 || cpu.registers[0] != g21GPUService)
            return GuestServiceAction::unsupported;
        if (cpu.registers[6] > g21MaximumCommands ||
            !queue_.submit(memory, cpu.registers[7],
                           static_cast<std::uint32_t>(cpu.registers[6]),
                           cpu.registers[2])) return GuestServiceAction::memoryFault;
        ++calls_;
        cpu.registers[0] = cpu.registers[2]; // synthetic fence acknowledgement
        return GuestServiceAction::resume;
    }
    std::uint32_t calls() const { return calls_; }
private:
    GuestGraphics21Queue &queue_;
    std::uint32_t calls_ = 0;
};
}

std::optional<G21Surface> rasterizeG21(const std::vector<G21Packet> &packets) {
    if (packets.size() < 2 || packets.size() > g21MaximumCommands ||
        packets.front().opcode != G21Opcode::clear ||
        packets.back().opcode != G21Opcode::present) return std::nullopt;
    std::uint32_t effect = 0;
    for (std::size_t i = 0; i < packets.size(); ++i) {
        const auto &p = packets[i];
        const bool middle = i != 0 && i + 1 != packets.size();
        switch (p.opcode) {
        case G21Opcode::clear:
            if (i != 0 || p.x || p.y || p.width || p.height || p.extra ||
                (p.data & 255) != 255) return std::nullopt;
            break;
        case G21Opcode::fill:
            if (!middle || !fits(p) || p.extra || (p.data & 255) != 255)
                return std::nullopt;
            break;
        case G21Opcode::sprite:
            if (!middle || !fits(p) || (p.data != 1 && p.data != 2) || p.extra)
                return std::nullopt;
            break;
        case G21Opcode::state:
            if (!middle || p.x > 1 || p.y > 3 || p.width || p.height ||
                (p.data & 255) != 255 || p.extra) return std::nullopt;
            effect = p.y;
            break;
        case G21Opcode::scissor:
            if (!middle || !fits(p) || p.data || p.extra) return std::nullopt;
            break;
        case G21Opcode::present:
            if (i + 1 != packets.size() || p.x || p.y || p.width ||
                p.height || p.data || p.extra) return std::nullopt;
            break;
        default: return std::nullopt;
        }
    }

    const Texture first = texture1();
    const Texture second = texture2();
    G21Surface surface;
    surface.pixels.resize(std::size_t(g21Width) * g21Height * 4);
    auto clear = color(packets.front().data);
    for (std::size_t i = 0; i < surface.pixels.size(); i += 4)
        for (unsigned c = 0; c < 4; ++c) surface.pixels[i + c] = clear[c];

    std::uint32_t scissorX = 0, scissorY = 0, scissorW = g21Width, scissorH = g21Height;
    std::uint32_t blendMode = 0;
    std::uint32_t tint = 0xffffffffu;
    bool seenFirst = false, seenSecond = false;
    for (const auto &p : packets) {
        if (p.opcode == G21Opcode::scissor) {
            scissorX = p.x; scissorY = p.y;
            scissorW = p.width; scissorH = p.height;
            ++surface.scissorChanges;
            continue;
        }
        if (p.opcode == G21Opcode::state) {
            blendMode = p.x;
            tint = p.data;
            ++surface.stateChanges;
            continue;
        }
        if (p.opcode != G21Opcode::fill && p.opcode != G21Opcode::sprite) continue;
        if (p.opcode == G21Opcode::fill) ++surface.rectangles;
        else {
            ++surface.sprites;
            if (p.data == 1) seenFirst = true;
            if (p.data == 2) seenSecond = true;
        }
        const auto fillColor = color(p.data);
        const auto tintColor = color(tint);
        const Texture &t = p.data == 2 ? second : first;
        for (std::uint32_t y = p.y; y < p.y + p.height; ++y) {
            if (y < scissorY || y >= scissorY + scissorH) continue;
            for (std::uint32_t x = p.x; x < p.x + p.width; ++x) {
                if (x < scissorX || x >= scissorX + scissorW) continue;
                const auto target = (std::size_t(y) * g21Width + x) * 4;
                if (p.opcode == G21Opcode::fill) {
                    for (unsigned c = 0; c < 4; ++c) surface.pixels[target + c] = fillColor[c];
                    continue;
                }
                const std::uint32_t tx = std::uint32_t(std::uint64_t(x - p.x) * 16 / p.width);
                const std::uint32_t ty = std::uint32_t(std::uint64_t(y - p.y) * 16 / p.height);
                const auto src = (std::size_t(ty) * 16 + tx) * 4;
                const auto alpha = std::uint32_t(t.rgba[src + 3]);
                for (unsigned k = 0; k < 3; ++k) {
                    const auto tinted = (std::uint32_t(t.rgba[src + k]) * tintColor[k] + 127) / 255;
                    const auto old = std::uint32_t(surface.pixels[target + k]);
                    const auto increment = (tinted * alpha + 127) / 255;
                    const auto output = blendMode == 0 ?
                        ((tinted * alpha + old * (255 - alpha) + 127) / 255) :
                        std::min(255u, old + increment);
                    surface.pixels[target + k] = static_cast<std::uint8_t>(output);
                }
                surface.pixels[target + 3] = 255;
            }
        }
    }
    surface.distinctTextures = std::uint32_t(seenFirst) + std::uint32_t(seenSecond);
    surface.effectMode = effect;
    std::uint64_t hash = fnvOffset;
    digest32(hash, g21Width); digest32(hash, g21Height);
    for (const auto &p : packets) {
        digest32(hash, static_cast<std::uint32_t>(p.opcode));
        digest32(hash, p.x); digest32(hash, p.y);
        digest32(hash, p.width); digest32(hash, p.height);
        digest32(hash, p.data); digest32(hash, p.extra);
    }
    for (std::uint8_t byte : surface.pixels) digest(hash, byte);
    surface.checksum = hash;
    return surface;
}

bool GuestGraphics21Queue::submit(const GuestMemory &memory, std::uint64_t address,
                                  std::uint32_t count, std::uint64_t fence) {
    if (count < 2 || count > g21MaximumCommands || pending_.size() >= 2 ||
        fence == 0 || fence <= lastSubmittedFence_ ||
        address > std::numeric_limits<std::uint64_t>::max() -
                  (std::uint64_t(count) * g21RecordBytes - 1)) return false;
    std::vector<G21Packet> packets;
    packets.reserve(count);
    for (std::uint32_t i = 0; i < count; ++i) {
        const auto offset = address + std::uint64_t(i) * g21RecordBytes;
        const auto op = memory.read64(offset);
        const auto xy = memory.read64(offset + 8);
        const auto wh = memory.read64(offset + 16);
        const auto data = memory.read64(offset + 24);
        if (!op || !xy || !wh || !data || *op < 1 || *op > 6) return false;
        packets.push_back({static_cast<G21Opcode>(*op),
                           std::uint32_t(*xy & mask32), std::uint32_t(*xy >> 32),
                           std::uint32_t(*wh & mask32), std::uint32_t(*wh >> 32),
                           std::uint32_t(*data & mask32), std::uint32_t(*data >> 32)});
    }
    auto surface = rasterizeG21(packets);
    if (!surface) return false;
    pending_.push_back({fence, std::move(*surface), std::move(packets)});
    lastSubmittedFence_ = fence;
    return true;
}

std::optional<G21Submission> GuestGraphics21Queue::take() {
    if (pending_.empty()) return std::nullopt;
    G21Submission frame = std::move(pending_.front());
    pending_.pop_front();
    completedFence_ = frame.fence;
    return frame;
}

std::optional<G21Diagnostic> runG21GuestFrame(std::uint32_t frameIndex,
                                               std::uint32_t requestedEffect) {
    if (requestedEffect > 3) return std::nullopt;
    const std::uint32_t firstX = 32 + 2 * (frameIndex % 90);
    const std::uint32_t secondX = 208 - (frameIndex % 90);
    const std::uint64_t fence = std::uint64_t(frameIndex) + 1;
    const auto commands = demoCommands();
    std::vector<std::uint8_t> code;
    patchXY(code, 4, firstX, 62);
    patchXY(code, 6, secondX, 78);
    patchXY(code, 5, 1, requestedEffect); // guest chooses postprocess shader
    movImm(code, 7, g21QueueAddress); // RDI: packet base
    movImm(code, 6, 10); // RSI: count
    movImm(code, 2, fence); // RDX: fence
    movImm(code, 0, g21GPUService); // RAX: synthetic service
    code.insert(code.end(), {0x0f, 0x05, 0xf4}); // SYSCALL, HLT

    GuestMemory memory;
    if (!memory.map(guestCode, code.size(), permission::read | permission::write) ||
        !memory.writeBytes(guestCode, code.data(), code.size()) ||
        !memory.protect(guestCode, code.size(), permission::read | permission::execute) ||
        !memory.map(g21QueueAddress, commands.size(), permission::read | permission::write) ||
        !memory.writeBytes(g21QueueAddress, commands.data(), commands.size()))
        return std::nullopt;
    GuestGraphics21Queue queue;
    G21Service service(queue);
    PortableX64Backend guest(std::move(memory), guestCode, &service);
    auto execution = guest.run(48);
    if (execution.stop != X64Stop::halted || execution.state.instructions != 15 ||
        !execution.stackRestored() || execution.rax() != fence ||
        service.calls() != 1 || queue.pending() != 1) return std::nullopt;
    auto submission = queue.take();
    if (!submission || submission->surface.rectangles != 2 ||
        submission->surface.sprites != 2 || submission->surface.distinctTextures != 2 ||
        submission->surface.stateChanges != 2 ||
        submission->surface.scissorChanges != 2 ||
        submission->surface.effectMode != requestedEffect ||
        submission->packets.size() != 10 ||
        queue.pending() != 0 || queue.completedFence() != fence) return std::nullopt;
    G21Diagnostic result;
    result.frame = std::move(*submission);
    result.guest = execution;
    result.serviceCalls = service.calls();
    result.firstSpriteX = firstX;
    result.secondSpriteX = secondX;
    result.queueDrained = queue.pending() == 0;
    result.completedFence = queue.completedFence();
    return result;
}
} // namespace misaki
