#include "../Include/GuestGraphics19.hpp"

#include <algorithm>
#include <array>
#include <limits>
#include <utility>

namespace misaki {
namespace {
constexpr std::uint32_t maximumDimension = 1024;
constexpr std::size_t maximumCommands = 64;
constexpr std::uint64_t fnvOffset = 14695981039346656037ULL;
constexpr std::uint64_t fnvPrime = 1099511628211ULL;
constexpr std::uint64_t guestGPUService = 0x330;
constexpr std::uint64_t guestCode = 0x2000, guestDescriptor = 0x4000;

void hashByte(std::uint64_t &hash, std::uint8_t value) {
    hash ^= value;
    hash *= fnvPrime;
}
void hash32(std::uint64_t &hash, std::uint32_t value) {
    for (int i = 0; i < 4; ++i) hashByte(hash, static_cast<std::uint8_t>(value >> (8 * i)));
}
std::array<std::uint8_t, 4> unpack(std::uint32_t rgba) {
    return {{static_cast<std::uint8_t>(rgba >> 24),
             static_cast<std::uint8_t>(rgba >> 16),
             static_cast<std::uint8_t>(rgba >> 8), static_cast<std::uint8_t>(rgba)}};
}
bool fits(const G19Command &c, const G19Scene &s) {
    return c.width != 0 && c.height != 0 && c.x < s.width && c.y < s.height &&
           c.width <= s.width - c.x && c.height <= s.height - c.y;
}
void blend(std::uint8_t *dest, const std::uint8_t *source) {
    const auto alpha = static_cast<std::uint32_t>(source[3]);
    for (unsigned i = 0; i < 3; ++i) {
        dest[i] = static_cast<std::uint8_t>((source[i] * alpha +
                     dest[i] * (255 - alpha) + 127) / 255);
    }
    dest[3] = 255;
}
void emit64(std::vector<std::uint8_t> &code, std::uint64_t value) {
    for (unsigned i = 0; i < 8; ++i)
        code.push_back(static_cast<std::uint8_t>(value >> (8 * i)));
}

class G19Service final : public IGuestServiceDispatcher {
public:
    GuestServiceAction dispatch(GuestMemory &memory, X64State &state,
                                std::uint32_t) override {
        if (state.registers[0] != guestGPUService || calls_ != 0)
            return GuestServiceAction::unsupported;
        const std::uint64_t pointer = state.registers[7]; // RDI
        if (pointer != guestDescriptor) return GuestServiceAction::memoryFault;
        if (pointer > std::numeric_limits<std::uint64_t>::max() - 16)
            return GuestServiceAction::memoryFault;
        const auto x = memory.read64(pointer);
        const auto y = memory.read64(pointer + 8);
        const auto texture = memory.read64(pointer + 16);
        if (!x || !y || !texture || *x > 272 || *y > 132 || *texture != 1)
            return GuestServiceAction::memoryFault;
        x_ = static_cast<std::uint32_t>(*x);
        y_ = static_cast<std::uint32_t>(*y);
        ++calls_;
        state.registers[0] = 1; // Toy service acknowledgement
        return GuestServiceAction::resume;
    }
    std::uint32_t calls() const { return calls_; }
    std::uint32_t x() const { return x_; }
    std::uint32_t y() const { return y_; }
private:
    std::uint32_t calls_ = 0, x_ = 0, y_ = 0;
};
}

G19Texture makeG19SpriteTexture() {
    G19Texture texture;
    texture.width = texture.height = 16;
    texture.rgba.resize(16 * 16 * 4);
    for (std::uint32_t y = 0; y < 16; ++y) {
        for (std::uint32_t x = 0; x < 16; ++x) {
            const bool rim = x == 0 || y == 0 || x == 15 || y == 15;
            const bool diamond = (x > 4 && x < 11 && y > 4 && y < 11);
            const std::uint32_t color = rim ? 0xFFFFFFFFu : diamond ? 0xFFE78DFFu :
                                  ((x / 4 + y / 4) % 2) ? 0x57D3E7FFu : 0xB45AE4FFu;
            auto rgba = unpack(color);
            // Transparent corners demonstrate alpha compositing in the framebuffer.
            if ((x < 2 && y < 2) || (x > 13 && y < 2) ||
                (x < 2 && y > 13) || (x > 13 && y > 13)) rgba[3] = 0;
            const auto index = (static_cast<std::size_t>(y) * 16 + x) * 4;
            for (unsigned c = 0; c < 4; ++c) texture.rgba[index + c] = rgba[c];
        }
    }
    return texture;
}

std::optional<G19Surface> rasterizeG19(const G19Scene &scene) {
    if (scene.width == 0 || scene.height == 0 || scene.width > maximumDimension ||
        scene.height > maximumDimension || scene.commands.size() < 2 ||
        scene.commands.size() > maximumCommands) return std::nullopt;
    const auto &texture = scene.spriteTexture;
    if (texture.width > 64 || texture.height > 64 ||
        (texture.width == 0) != (texture.height == 0) ||
        texture.rgba.size() != std::size_t(texture.width) * texture.height * 4)
        return std::nullopt;
    bool hasTextureCommand = false;
    for (std::size_t i = 0; i < scene.commands.size(); ++i) {
        const auto &c = scene.commands[i];
        switch (c.opcode) {
        case G19Opcode::clear:
            if (i != 0 || c.x || c.y || c.width || c.height || c.textureID ||
                (c.rgba & 255) != 255) return std::nullopt;
            break;
        case G19Opcode::fill:
            if (i == 0 || i + 1 == scene.commands.size() || !fits(c, scene) ||
                c.textureID != 0 || (c.rgba & 255) != 255) return std::nullopt;
            break;
        case G19Opcode::sprite:
            if (i == 0 || i + 1 == scene.commands.size() || !fits(c, scene) ||
                c.rgba != 0 || c.textureID != 1 || !texture.width) return std::nullopt;
            hasTextureCommand = true;
            break;
        case G19Opcode::present:
            if (i + 1 != scene.commands.size() || c.x || c.y || c.width ||
                c.height || c.rgba || c.textureID) return std::nullopt;
            break;
        default: return std::nullopt;
        }
    }
    if (scene.commands.front().opcode != G19Opcode::clear ||
        scene.commands.back().opcode != G19Opcode::present ||
        (!hasTextureCommand && texture.width != 0)) return std::nullopt;

    G19Surface surface;
    surface.width = scene.width;
    surface.height = scene.height;
    surface.pixels.resize(std::size_t(scene.width) * scene.height * 4);
    const auto clear = unpack(scene.commands.front().rgba);
    for (std::size_t i = 0; i < surface.pixels.size(); i += 4)
        std::copy(clear.begin(), clear.end(), surface.pixels.begin() + i);
    for (const auto &command : scene.commands) {
        if (command.opcode == G19Opcode::fill) ++surface.filledRects;
        if (command.opcode == G19Opcode::sprite) ++surface.texturedSprites;
        if (command.opcode != G19Opcode::fill && command.opcode != G19Opcode::sprite)
            continue;
        const auto color = unpack(command.rgba);
        for (std::uint32_t dy = 0; dy < command.height; ++dy) {
            for (std::uint32_t dx = 0; dx < command.width; ++dx) {
                const auto dest = (std::size_t(command.y + dy) * scene.width + command.x + dx) * 4;
                if (command.opcode == G19Opcode::fill) {
                    std::copy(color.begin(), color.end(), surface.pixels.begin() + dest);
                } else {
                    const auto tx = static_cast<std::uint32_t>(std::uint64_t(dx) * texture.width / command.width);
                    const auto ty = static_cast<std::uint32_t>(std::uint64_t(dy) * texture.height / command.height);
                    const auto src = (std::size_t(ty) * texture.width + tx) * 4;
                    blend(surface.pixels.data() + dest, texture.rgba.data() + src);
                }
            }
        }
    }
    std::uint64_t checksum = fnvOffset;
    hash32(checksum, surface.width);
    hash32(checksum, surface.height);
    for (const auto &c : scene.commands) {
        hash32(checksum, static_cast<std::uint32_t>(c.opcode));
        hash32(checksum, c.x); hash32(checksum, c.y);
        hash32(checksum, c.width); hash32(checksum, c.height);
        hash32(checksum, c.rgba); hash32(checksum, c.textureID);
    }
    for (const auto pixel : surface.pixels) hashByte(checksum, pixel);
    surface.checksum = checksum;
    return surface;
}

std::optional<G19Diagnostic> runG19GuestFrame(std::uint32_t frameIndex) {
    // The CPU, not Swift, stores X into a guest descriptor and submits it.
    // One complete cycle lasts 90 frames at whatever display cadence is used.
    const auto x = static_cast<std::uint64_t>(32 + 2 * (frameIndex % 90));
    std::vector<std::uint8_t> code{0x48, 0xBF}; // mov rdi, descriptor
    emit64(code, guestDescriptor);
    code.push_back(0x48); code.push_back(0xB8); // mov rax, sprite X
    emit64(code, x);
    code.push_back(0x48); code.push_back(0x89); code.push_back(0x07); // mov [rdi],rax
    code.push_back(0x48); code.push_back(0xB8); // mov rax, service ID
    emit64(code, guestGPUService);
    code.push_back(0x0F); code.push_back(0x05); // syscall
    code.push_back(0xF4); // hlt

    GuestMemory memory;
    if (!memory.map(guestCode, code.size(), permission::read | permission::write) ||
        !memory.writeBytes(guestCode, code.data(), code.size()) ||
        !memory.protect(guestCode, code.size(), permission::read | permission::execute) ||
        !memory.map(guestDescriptor, 24, permission::read | permission::write) ||
        !memory.write64(guestDescriptor + 8, 78) ||
        !memory.write64(guestDescriptor + 16, 1)) return std::nullopt;

    G19Service gpuService;
    PortableX64Backend backend(std::move(memory), guestCode, &gpuService);
    auto result = backend.run(20);
    if (result.stop != X64Stop::halted || result.state.instructions != 6 ||
        result.rax() != 1 || !result.stackRestored() || gpuService.calls() != 1)
        return std::nullopt;

    G19Scene scene;
    scene.spriteTexture = makeG19SpriteTexture();
    scene.commands = {
        {G19Opcode::clear, 0, 0, 0, 0, 0x0C1426FF, 0},
        {G19Opcode::fill, 16, 18, 288, 144, 0x263A61FF, 0},
        {G19Opcode::fill, 24, 28, 272, 4, 0x57D3E7FF, 0},
        {G19Opcode::fill, 30, 145, 260, 8, 0x162544FF, 0},
        {G19Opcode::sprite, gpuService.x(), gpuService.y(), 48, 48, 0, 1},
        {G19Opcode::fill, 14, 168, 292, 8, 0x354E7FFF, 0},
        {G19Opcode::present, 0, 0, 0, 0, 0, 0}
    };
    auto surface = rasterizeG19(scene);
    if (!surface || surface->filledRects != 4 || surface->texturedSprites != 1)
        return std::nullopt;
    G19Diagnostic d;
    d.surface = std::move(*surface);
    d.execution = result;
    d.serviceCalls = gpuService.calls();
    d.spriteX = gpuService.x();
    d.spriteY = gpuService.y();
    d.commandCount = static_cast<std::uint32_t>(scene.commands.size());
    return d;
}
} // namespace misaki
