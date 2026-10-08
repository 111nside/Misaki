#include "../Include/GuestCPU.hpp"

#include <limits>
#include <utility>

namespace misaki {
namespace {
std::uint16_t read16(const std::uint8_t *b, std::size_t at) {
    return std::uint16_t(b[at]) | (std::uint16_t(b[at + 1]) << 8);
}
std::uint32_t read32(const std::uint8_t *b, std::size_t at) {
    std::uint32_t value = 0;
    for (unsigned i = 0; i < 4; ++i) value |= std::uint32_t(b[at + i]) << (8 * i);
    return value;
}
std::uint64_t read64(const std::uint8_t *b, std::size_t at) {
    std::uint64_t value = 0;
    for (unsigned i = 0; i < 8; ++i) value |= std::uint64_t(b[at + i]) << (8 * i);
    return value;
}
void put(std::vector<std::uint8_t> &bytes, std::size_t at,
         std::uint64_t value, unsigned count) {
    for (unsigned i = 0; i < count; ++i)
        bytes[at + i] = static_cast<std::uint8_t>(value >> (8 * i));
}
constexpr std::uint64_t stackBase = 0x70000000;
constexpr std::size_t stackSize = 64 * 1024;
}

std::optional<GuestLoadedImage> loadGuestELF(const std::uint8_t *bytes,
                                             std::size_t length,
                                             std::uint64_t loadBias,
                                             GuestMemory &memory) {
    const auto header = inspectELF64(bytes, length);
    if (!header || !bytes) return std::nullopt;
    if (header->entryPoint > std::numeric_limits<std::uint64_t>::max() - loadBias)
        return std::nullopt;
    const auto table = read64(bytes, 32);
    const auto width = read16(bytes, 54);
    const auto count = read16(bytes, 56);
    GuestMemory candidate = memory; // No partial mappings on malformed input.
    GuestLoadedImage image;
    image.entry = header->entryPoint + loadBias;
    for (unsigned i = 0; i < count; ++i) {
        const std::size_t at = static_cast<std::size_t>(table) + std::size_t(i) * width;
        if (read32(bytes, at) != 1) continue;
        const auto flags = read32(bytes, at + 4);
        const auto fileOffset = read64(bytes, at + 8);
        const auto vaddr = read64(bytes, at + 16);
        const auto fileSize = read64(bytes, at + 32);
        const auto memSize = read64(bytes, at + 40);
        if (flags > 7 || memSize == 0 ||
            vaddr > std::numeric_limits<std::uint64_t>::max() - loadBias)
            return std::nullopt;
        const auto address = vaddr + loadBias;
        if (memSize > std::numeric_limits<std::size_t>::max() ||
            fileOffset > length || fileSize > length - fileOffset ||
            address > std::numeric_limits<std::uint64_t>::max() - memSize)
            return std::nullopt;
        const auto size = static_cast<std::size_t>(memSize);
        if (!candidate.map(address, size, permission::read | permission::write))
            return std::nullopt;
        if (fileSize != 0 && !candidate.writeBytes(address, bytes + fileOffset,
                                                  static_cast<std::size_t>(fileSize)))
            return std::nullopt;
        std::uint8_t perms = 0;
        if ((flags & 4) != 0) perms |= permission::read;
        if ((flags & 2) != 0) perms |= permission::write;
        if ((flags & 1) != 0) perms |= permission::execute;
        if (!candidate.protect(address, size, perms)) return std::nullopt;
        ++image.segments;
    }
    if (!candidate.isExecutable(image.entry) || image.segments == 0) return std::nullopt;
    memory = std::move(candidate);
    return image;
}

GuestCPU::GuestCPU(GuestMemory memory, std::uint64_t entry)
    : memory_(std::move(memory)), rip_(entry) {
    initialSP_ = stackBase + stackSize;
    rsp_ = initialSP_;
    ready_ = memory_.map(stackBase, stackSize, permission::read | permission::write);
}

bool GuestCPU::fetch(std::uint8_t &value) {
    if (rip_ == std::numeric_limits<std::uint64_t>::max()) return false;
    const auto byte = memory_.fetch8(rip_);
    if (!byte) return false;
    value = *byte;
    ++rip_;
    return true;
}
bool GuestCPU::fetch32(std::uint32_t &value) {
    value = 0;
    for (unsigned i = 0; i < 4; ++i) {
        std::uint8_t part = 0;
        if (!fetch(part)) return false;
        value |= std::uint32_t(part) << (8 * i);
    }
    return true;
}
bool GuestCPU::fetch64(std::uint64_t &value) {
    value = 0;
    for (unsigned i = 0; i < 8; ++i) {
        std::uint8_t part = 0;
        if (!fetch(part)) return false;
        value |= std::uint64_t(part) << (8 * i);
    }
    return true;
}
bool GuestCPU::push(std::uint64_t value) {
    if (rsp_ < 8 || !memory_.write64(rsp_ - 8, value)) return false;
    rsp_ -= 8;
    return true;
}
bool GuestCPU::pop(std::uint64_t &value) {
    const auto result = memory_.read64(rsp_);
    if (!result || rsp_ > std::numeric_limits<std::uint64_t>::max() - 8)
        return false;
    value = *result;
    rsp_ += 8;
    return true;
}
std::optional<GuestStop> GuestCPU::step() {
    std::uint8_t opcode = 0;
    if (!fetch(opcode)) return GuestStop::memoryFault;
    switch (opcode) {
    case 0x90: break; // NOP
    case 0xF4: ++instructions_; return GuestStop::halted; // test HLT
    case 0x48: { // REX.W
        std::uint8_t second = 0;
        if (!fetch(second)) return GuestStop::memoryFault;
        if (second == 0xB8) { // MOV RAX, imm64
            if (!fetch64(rax_)) return GuestStop::memoryFault;
        } else if (second == 0x05 || second == 0x3D) { // ADD/CMP RAX, imm32
            std::uint32_t imm = 0;
            if (!fetch32(imm)) return GuestStop::memoryFault;
            const auto rhs = static_cast<std::uint64_t>(static_cast<std::int64_t>(static_cast<std::int32_t>(imm)));
            if (second == 0x05) { rax_ += rhs; zf_ = rax_ == 0; }
            else zf_ = rax_ == rhs;
        } else if (second == 0xFF) { // CALL qword ptr [RIP+disp32] only
            std::uint8_t modrm = 0;
            std::uint32_t disp = 0;
            if (!fetch(modrm) || !fetch32(disp)) return GuestStop::memoryFault;
            if (modrm != 0x15) return GuestStop::invalidOpcode;
            const auto slot = rip_ + static_cast<std::uint64_t>(static_cast<std::int64_t>(static_cast<std::int32_t>(disp)));
            const auto target = memory_.read64(slot);
            if (!target) return GuestStop::memoryFault;
            if (!push(rip_)) return GuestStop::stackFault;
            rip_ = *target;
        } else return GuestStop::invalidOpcode;
        break;
    }
    case 0xE8: { // CALL rel32
        std::uint32_t offset = 0;
        if (!fetch32(offset)) return GuestStop::memoryFault;
        if (!push(rip_)) return GuestStop::stackFault;
        rip_ += static_cast<std::uint64_t>(static_cast<std::int64_t>(static_cast<std::int32_t>(offset)));
        break;
    }
    case 0xC3: { // RET
        std::uint64_t target = 0;
        if (!pop(target)) return GuestStop::stackFault;
        rip_ = target;
        break;
    }
    case 0x74: case 0x75: { // JE/JNE rel8
        std::uint8_t delta = 0;
        if (!fetch(delta)) return GuestStop::memoryFault;
        if ((opcode == 0x74 && zf_) || (opcode == 0x75 && !zf_))
            rip_ += static_cast<std::uint64_t>(static_cast<std::int64_t>(static_cast<std::int8_t>(delta)));
        break;
    }
    default: return GuestStop::invalidOpcode;
    }
    ++instructions_;
    return std::nullopt;
}
GuestCPUResult GuestCPU::result(GuestStop status) const {
    GuestCPUResult r;
    r.stop = status;
    r.rax = rax_;
    r.rip = rip_;
    r.rsp = rsp_;
    r.instructions = instructions_;
    r.zeroFlag = zf_;
    r.stackRestored = rsp_ == initialSP_;
    return r;
}
GuestCPUResult GuestCPU::run(std::uint32_t maxSteps) {
    if (!ready_) return result(GuestStop::stackFault);
    // Deliberate instruction cap: test guest code cannot run indefinitely.
    if (maxSteps == 0 || maxSteps > 100000) return result(GuestStop::stepLimit);
    for (std::uint32_t i = 0; i < maxSteps; ++i) {
        auto stop = step();
        if (stop) return result(*stop);
    }
    return result(GuestStop::stepLimit);
}

std::vector<std::uint8_t> makeGuestCPUFixture() {
    // Self-authored ET_EXEC: code (CALL [RIP+0xF9]; HLT), RW import slot.
    std::vector<std::uint8_t> elf(0x208, 0);
    elf[0] = 0x7f; elf[1] = 'E'; elf[2] = 'L'; elf[3] = 'F';
    elf[4] = 2; elf[5] = 1; elf[6] = 1;
    put(elf, 16, 2, 2); put(elf, 18, 0x3e, 2); put(elf, 20, 1, 4);
    put(elf, 24, 0x1000, 8); put(elf, 32, 64, 8);
    put(elf, 52, 64, 2); put(elf, 54, 56, 2); put(elf, 56, 2, 2);
    // PT_LOAD executable region
    put(elf, 64, 1, 4); put(elf, 68, 5, 4); put(elf, 72, 0x100, 8);
    put(elf, 80, 0x1000, 8); put(elf, 96, 8, 8); put(elf, 104, 8, 8);
    put(elf, 112, 0x100, 8);
    // PT_LOAD import slot
    put(elf, 120, 1, 4); put(elf, 124, 6, 4); put(elf, 128, 0x200, 8);
    put(elf, 136, 0x1100, 8); put(elf, 152, 8, 8); put(elf, 160, 8, 8);
    put(elf, 168, 0x100, 8);
    const std::uint8_t code[] = {0x48, 0xFF, 0x15, 0xF9, 0, 0, 0, 0xF4};
    for (std::size_t i = 0; i < sizeof(code); ++i) elf[0x100 + i] = code[i];
    return elf;
}

std::optional<GuestCPUDiagnostic> runGuestCPUDiagnostic() {
    const auto elf = makeGuestCPUFixture();
    GuestMemory memory;
    const auto image = loadGuestELF(elf.data(), elf.size(), 0, memory);
    if (!image || image->segments != 2) return std::nullopt;
    // Native guest code module: MOV RAX,40; ADD RAX,2; RET.
    constexpr std::uint8_t func[] = {
        0x48, 0xB8, 40, 0, 0, 0, 0, 0, 0, 0,
        0x48, 0x05, 2, 0, 0, 0, 0xC3
    };
    if (!memory.map(0x3000, sizeof(func), permission::read | permission::write) ||
        !memory.writeBytes(0x3000, func, sizeof(func)) ||
        !memory.protect(0x3000, sizeof(func), permission::read | permission::execute))
        return std::nullopt;
    ModuleRegistry registry;
    if (!registry.registerModule({"libMisakiNativeTest", {{"return42", 0x3000}}}) ||
        !registry.bindImport(memory, 0x1100, "libMisakiNativeTest", "return42") ||
        !memory.protect(0x1100, 8, permission::read))
        return std::nullopt;
    const auto linked = memory.read64(0x1100);
    if (!linked || *linked != 0x3000) return std::nullopt;
    const bool readOnly = !memory.write64(0x1100, 0);
    GuestCPU cpu(std::move(memory), image->entry);
    GuestCPUDiagnostic report;
    report.cpu = cpu.run(20);
    report.segments = image->segments;
    report.imports = 1;
    report.linkedAddress = *linked;
    report.importReadOnly = readOnly;
    return report;
}
} // namespace misaki
