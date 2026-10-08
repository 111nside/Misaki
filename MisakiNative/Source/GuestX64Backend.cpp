#include "../Include/GuestX64Backend.hpp"

#include <limits>
#include <utility>

namespace misaki {
namespace {
constexpr std::uint64_t stackBase = 0x71000000;
constexpr std::size_t stackSize = 64 * 1024;
constexpr std::uint64_t signBit = std::uint64_t(1) << 63;

std::uint64_t extend32(std::uint32_t number) {
    return static_cast<std::uint64_t>(static_cast<std::int64_t>(
        static_cast<std::int32_t>(number)));
}
std::uint64_t extend8(std::uint8_t number) {
    return static_cast<std::uint64_t>(static_cast<std::int64_t>(
        static_cast<std::int8_t>(number)));
}
std::uint64_t addRelative(std::uint64_t base, std::int64_t displacement) {
    return base + static_cast<std::uint64_t>(displacement); // x86-64 wraps
}
std::uint64_t magnitude(std::int64_t number) {
    const auto bits = static_cast<std::uint64_t>(number);
    return number >= 0 ? bits : std::uint64_t(0) - bits;
}
}

PortableX64Backend::PortableX64Backend(GuestMemory memory, std::uint64_t entry)
    : memory_(std::move(memory)) {
    cpu_.rip = entry;
    cpu_.initialStack = stackBase + stackSize;
    cpu_.registers[4] = cpu_.initialStack;
    stackReady_ = memory_.map(stackBase, stackSize, permission::read | permission::write);
}

bool PortableX64Backend::fetch8(std::uint8_t &out) {
    const auto value = memory_.fetch8(cpu_.rip);
    if (!value || cpu_.rip == std::numeric_limits<std::uint64_t>::max()) return false;
    out = *value;
    ++cpu_.rip;
    return true;
}
bool PortableX64Backend::fetch32(std::uint32_t &out) {
    out = 0;
    for (unsigned index = 0; index < 4; ++index) {
        std::uint8_t byte = 0;
        if (!fetch8(byte)) return false;
        out |= std::uint32_t(byte) << (8 * index);
    }
    return true;
}
bool PortableX64Backend::fetch64(std::uint64_t &out) {
    out = 0;
    for (unsigned index = 0; index < 8; ++index) {
        std::uint8_t byte = 0;
        if (!fetch8(byte)) return false;
        out |= std::uint64_t(byte) << (8 * index);
    }
    return true;
}

bool PortableX64Backend::decodeModRM(std::uint8_t rex,
                                     std::uint64_t immediateBytes, ModRM &out) {
    std::uint8_t modrm = 0;
    if (!fetch8(modrm)) return false;
    const unsigned mode = modrm >> 6;
    const unsigned rawReg = (modrm >> 3) & 7;
    const unsigned rawRM = modrm & 7;
    out.reg = rawReg + ((rex & 4) ? 8 : 0);
    out.group = rawReg;
    if (mode == 3) {
        out.rm = Operand{true, rawRM + ((rex & 1) ? 8u : 0u), 0};
        return true;
    }
    std::uint64_t base = 0;
    std::uint64_t index = 0;
    bool ripRelative = false;
    bool needDisp32 = false;
    if (rawRM == 4) {
        std::uint8_t sib = 0;
        if (!fetch8(sib)) return false;
        const unsigned scale = sib >> 6;
        const unsigned rawIndex = (sib >> 3) & 7;
        const unsigned rawBase = sib & 7;
        if (rawIndex != 4 || (rex & 2) != 0) {
            const unsigned n = rawIndex + ((rex & 2) ? 8u : 0u);
            index = cpu_.registers[n] * (std::uint64_t(1) << scale);
        }
        if (mode == 0 && rawBase == 5) needDisp32 = true;
        else base = cpu_.registers[rawBase + ((rex & 1) ? 8u : 0u)];
    } else if (mode == 0 && rawRM == 5) {
        needDisp32 = true;
        ripRelative = true;
    } else {
        base = cpu_.registers[rawRM + ((rex & 1) ? 8u : 0u)];
    }
    std::int64_t displacement = 0;
    if (mode == 1) {
        std::uint8_t raw = 0;
        if (!fetch8(raw)) return false;
        displacement = static_cast<std::int8_t>(raw);
    } else if (mode == 2 || needDisp32) {
        std::uint32_t raw = 0;
        if (!fetch32(raw)) return false;
        displacement = static_cast<std::int32_t>(raw);
    }
    if (ripRelative) base = cpu_.rip + immediateBytes;
    out.rm = Operand{false, 0, addRelative(base + index, displacement)};
    return true;
}

std::optional<std::uint64_t> PortableX64Backend::readOperand(const Operand &op) const {
    if (op.isRegister) return cpu_.registers[op.index];
    return memory_.read64(op.address);
}
bool PortableX64Backend::writeOperand(const Operand &op, std::uint64_t value) {
    if (op.isRegister) { cpu_.registers[op.index] = value; return true; }
    return memory_.write64(op.address, value);
}
bool PortableX64Backend::push(std::uint64_t value) {
    const auto sp = cpu_.registers[4];
    if (sp < 8 || !memory_.write64(sp - 8, value)) return false;
    cpu_.registers[4] = sp - 8;
    return true;
}
bool PortableX64Backend::pop(std::uint64_t &value) {
    const auto sp = cpu_.registers[4];
    const auto saved = memory_.read64(sp);
    if (!saved || sp > std::numeric_limits<std::uint64_t>::max() - 8) return false;
    value = *saved;
    cpu_.registers[4] = sp + 8;
    return true;
}

void PortableX64Backend::logicFlags(std::uint64_t value) {
    cpu_.zeroFlag = value == 0;
    cpu_.signFlag = (value & signBit) != 0;
    const auto lowByte = static_cast<std::uint8_t>(value);
    unsigned parity = 0;
    for (unsigned i = 0; i < 8; ++i) parity ^= (lowByte >> i) & 1;
    cpu_.parityFlag = parity == 0;
    cpu_.carryFlag = false;
    cpu_.overflowFlag = false;
}
void PortableX64Backend::addFlags(std::uint64_t lhs, std::uint64_t rhs,
                                   std::uint64_t value) {
    logicFlags(value);
    cpu_.carryFlag = value < lhs;
    cpu_.overflowFlag = ((~(lhs ^ rhs) & (lhs ^ value)) & signBit) != 0;
}
void PortableX64Backend::subFlags(std::uint64_t lhs, std::uint64_t rhs,
                                   std::uint64_t value) {
    logicFlags(value);
    cpu_.carryFlag = lhs < rhs;
    cpu_.overflowFlag = (((lhs ^ rhs) & (lhs ^ value)) & signBit) != 0;
}
bool PortableX64Backend::branchCondition(unsigned code) const {
    switch (code & 15) {
    case 0: return cpu_.overflowFlag;
    case 1: return !cpu_.overflowFlag;
    case 2: return cpu_.carryFlag;
    case 3: return !cpu_.carryFlag;
    case 4: return cpu_.zeroFlag;
    case 5: return !cpu_.zeroFlag;
    case 6: return cpu_.carryFlag || cpu_.zeroFlag;
    case 7: return !cpu_.carryFlag && !cpu_.zeroFlag;
    case 8: return cpu_.signFlag;
    case 9: return !cpu_.signFlag;
    case 10: return cpu_.parityFlag;
    case 11: return !cpu_.parityFlag;
    case 12: return cpu_.signFlag != cpu_.overflowFlag;
    case 13: return cpu_.signFlag == cpu_.overflowFlag;
    case 14: return cpu_.zeroFlag || (cpu_.signFlag != cpu_.overflowFlag);
    default: return !cpu_.zeroFlag && (cpu_.signFlag == cpu_.overflowFlag);
    }
}

std::optional<X64Stop> PortableX64Backend::step() {
    std::uint8_t opcode = 0;
    if (!fetch8(opcode)) return X64Stop::memoryFault;
    std::uint8_t rex = 0;
    unsigned prefixes = 0;
    while (opcode >= 0x40 && opcode <= 0x4f) {
        rex = opcode;
        if (++prefixes > 4 || !fetch8(opcode)) return X64Stop::memoryFault;
    }
    const bool rexW = (rex & 8) != 0;
    if (opcode == 0x90 && rex == 0) return std::nullopt;
    if (opcode == 0xf4 && rex == 0) return X64Stop::halted;
    if (opcode >= 0x50 && opcode <= 0x57) {
        unsigned reg = opcode - 0x50 + ((rex & 1) ? 8u : 0u);
        if (!push(cpu_.registers[reg])) return X64Stop::stackFault;
        return std::nullopt;
    }
    if (opcode >= 0x58 && opcode <= 0x5f) {
        unsigned reg = opcode - 0x58 + ((rex & 1) ? 8u : 0u);
        if (!pop(cpu_.registers[reg])) return X64Stop::stackFault;
        return std::nullopt;
    }
    if (rex == 0 && opcode == 0xc3) {
        std::uint64_t returnAddress = 0;
        if (!pop(returnAddress)) return X64Stop::stackFault;
        cpu_.rip = returnAddress;
        return std::nullopt;
    }
    if (rex == 0 && opcode == 0xc9) { // LEAVE
        cpu_.registers[4] = cpu_.registers[5];
        if (!pop(cpu_.registers[5])) return X64Stop::stackFault;
        return std::nullopt;
    }
    if (rex == 0 && opcode == 0xe8) {
        std::uint32_t displacement = 0;
        if (!fetch32(displacement)) return X64Stop::memoryFault;
        if (!push(cpu_.rip)) return X64Stop::stackFault;
        cpu_.rip += extend32(displacement);
        return std::nullopt;
    }
    if (rex == 0 && opcode == 0xe9) {
        std::uint32_t displacement = 0;
        if (!fetch32(displacement)) return X64Stop::memoryFault;
        cpu_.rip += extend32(displacement);
        return std::nullopt;
    }
    if (rex == 0 && opcode == 0xeb) {
        std::uint8_t displacement = 0;
        if (!fetch8(displacement)) return X64Stop::memoryFault;
        cpu_.rip += extend8(displacement);
        return std::nullopt;
    }
    if (rex == 0 && opcode >= 0x70 && opcode <= 0x7f) {
        std::uint8_t displacement = 0;
        if (!fetch8(displacement)) return X64Stop::memoryFault;
        if (branchCondition(opcode)) cpu_.rip += extend8(displacement);
        return std::nullopt;
    }
    if (rex == 0 && opcode == 0x68) {
        std::uint32_t immediate = 0;
        if (!fetch32(immediate)) return X64Stop::memoryFault;
        if (!push(extend32(immediate))) return X64Stop::stackFault;
        return std::nullopt;
    }
    if (rex == 0 && opcode == 0x6a) {
        std::uint8_t immediate = 0;
        if (!fetch8(immediate)) return X64Stop::memoryFault;
        if (!push(extend8(immediate))) return X64Stop::stackFault;
        return std::nullopt;
    }
    if (opcode == 0xff) {
        ModRM d;
        if (!decodeModRM(rex, 0, d)) return X64Stop::memoryFault;
        if (d.group != 2 && d.group != 4 && d.group != 6) return X64Stop::invalidOpcode;
        auto value = readOperand(d.rm);
        if (!value) return X64Stop::memoryFault;
        if (d.group == 2) {
            if (!push(cpu_.rip)) return X64Stop::stackFault;
            cpu_.rip = *value;
        } else if (d.group == 4) {
            cpu_.rip = *value;
        } else if (!push(*value)) return X64Stop::stackFault;
        return std::nullopt;
    }
    if (opcode == 0x0f) {
        std::uint8_t second = 0;
        if (!fetch8(second)) return X64Stop::memoryFault;
        if (rex == 0 && second >= 0x80 && second <= 0x8f) {
            std::uint32_t displacement = 0;
            if (!fetch32(displacement)) return X64Stop::memoryFault;
            if (branchCondition(second)) cpu_.rip += extend32(displacement);
            return std::nullopt;
        }
        if (!rexW || second != 0xaf) return X64Stop::invalidOpcode;
        ModRM d;
        if (!decodeModRM(rex, 0, d)) return X64Stop::memoryFault;
        auto rhs = readOperand(d.rm);
        if (!rhs) return X64Stop::memoryFault;
        const auto lhs = cpu_.registers[d.reg];
        cpu_.registers[d.reg] = lhs * (*rhs); // low 64 bits, modulo 2^64
        const auto a = static_cast<std::int64_t>(lhs);
        const auto b = static_cast<std::int64_t>(*rhs);
        const bool negative = (a < 0) != (b < 0);
        const std::uint64_t limit = negative ? signBit : (signBit - 1);
        const auto ma = magnitude(a), mb = magnitude(b);
        const bool overflow = ma != 0 && mb > limit / ma;
        cpu_.carryFlag = overflow;
        cpu_.overflowFlag = overflow;
        return std::nullopt;
    }
    if (!rexW) return X64Stop::invalidOpcode;
    if (opcode >= 0xb8 && opcode <= 0xbf) {
        std::uint64_t immediate = 0;
        if (!fetch64(immediate)) return X64Stop::memoryFault;
        const unsigned reg = opcode - 0xb8 + ((rex & 1) ? 8u : 0u);
        cpu_.registers[reg] = immediate;
        return std::nullopt;
    }
    if (opcode == 0x05 || opcode == 0x2d || opcode == 0x3d) {
        std::uint32_t immediate = 0;
        if (!fetch32(immediate)) return X64Stop::memoryFault;
        const auto rhs = extend32(immediate);
        const auto lhs = cpu_.registers[0];
        if (opcode == 0x05) {
            cpu_.registers[0] = lhs + rhs;
            addFlags(lhs, rhs, cpu_.registers[0]);
        } else {
            const auto result = lhs - rhs;
            if (opcode == 0x2d) cpu_.registers[0] = result;
            subFlags(lhs, rhs, result);
        }
        return std::nullopt;
    }
    switch (opcode) {
    case 0x89: case 0x8b: case 0x8d:
    case 0x01: case 0x03: case 0x29: case 0x2b:
    case 0x39: case 0x3b: case 0x31: case 0x33: case 0x85:
    case 0x81: case 0x83: case 0xc7: break;
    default: return X64Stop::invalidOpcode;
    }
    ModRM d;
    const std::uint64_t trailingBytes = opcode == 0x81 || opcode == 0xc7 ? 4 :
                                        opcode == 0x83 ? 1 : 0;
    if (!decodeModRM(rex, trailingBytes, d)) return X64Stop::memoryFault;
    if (opcode == 0x8d) {
        if (d.rm.isRegister) return X64Stop::invalidOpcode;
        cpu_.registers[d.reg] = d.rm.address;
        return std::nullopt;
    }
    if (opcode == 0x89) {
        return writeOperand(d.rm, cpu_.registers[d.reg]) ? std::nullopt :
               std::optional<X64Stop>(X64Stop::memoryFault);
    }
    if (opcode == 0x8b) {
        const auto value = readOperand(d.rm);
        if (!value) return X64Stop::memoryFault;
        cpu_.registers[d.reg] = *value;
        return std::nullopt;
    }
    if (opcode == 0x81 || opcode == 0x83 || opcode == 0xc7) {
        std::uint64_t immediate = 0;
        if (opcode == 0x83) {
            std::uint8_t raw = 0;
            if (!fetch8(raw)) return X64Stop::memoryFault;
            immediate = extend8(raw);
        } else {
            std::uint32_t raw = 0;
            if (!fetch32(raw)) return X64Stop::memoryFault;
            immediate = extend32(raw);
        }
        if (opcode == 0xc7) {
            if (d.group != 0) return X64Stop::invalidOpcode;
            return writeOperand(d.rm, immediate) ? std::nullopt :
                   std::optional<X64Stop>(X64Stop::memoryFault);
        }
        if (d.group != 0 && d.group != 5 && d.group != 7)
            return X64Stop::invalidOpcode;
        const auto lhs = readOperand(d.rm);
        if (!lhs) return X64Stop::memoryFault;
        if (d.group == 0) {
            const auto value = *lhs + immediate;
            if (!writeOperand(d.rm, value)) return X64Stop::memoryFault;
            addFlags(*lhs, immediate, value);
        } else {
            const auto value = *lhs - immediate;
            if (d.group == 5 && !writeOperand(d.rm, value)) return X64Stop::memoryFault;
            subFlags(*lhs, immediate, value);
        }
        return std::nullopt;
    }
    const bool regDestination = opcode == 0x03 || opcode == 0x2b ||
                                 opcode == 0x3b || opcode == 0x33;
    const auto operandValue = readOperand(d.rm);
    if (!operandValue) return X64Stop::memoryFault;
    const auto lhs = regDestination ? cpu_.registers[d.reg] : *operandValue;
    const auto rhs = regDestination ? *operandValue : cpu_.registers[d.reg];
    const auto target = regDestination ? Operand{true, d.reg, 0} : d.rm;
    const bool isTest = opcode == 0x85;
    const bool isCmp = opcode == 0x39 || opcode == 0x3b;
    std::uint64_t value = 0;
    if (opcode == 0x01 || opcode == 0x03) value = lhs + rhs;
    else if (opcode == 0x29 || opcode == 0x2b || isCmp) value = lhs - rhs;
    else if (isTest) value = lhs & rhs;
    else value = lhs ^ rhs;
    if (!isTest && !isCmp && !writeOperand(target, value)) return X64Stop::memoryFault;
    if (opcode == 0x01 || opcode == 0x03) addFlags(lhs, rhs, value);
    else if (opcode == 0x29 || opcode == 0x2b || isCmp) subFlags(lhs, rhs, value);
    else logicFlags(value);
    return std::nullopt;
}

X64ExecutionResult PortableX64Backend::result(X64Stop stop) const {
    return X64ExecutionResult{stop, cpu_};
}
X64ExecutionResult PortableX64Backend::run(std::uint32_t maxInstructions) {
    if (terminated_) return result(lastStop_);
    if (!stackReady_) return result(X64Stop::stackFault);
    if (maxInstructions == 0 || maxInstructions > 100000) return result(X64Stop::stepLimit);
    for (std::uint32_t i = 0; i < maxInstructions; ++i) {
        const auto stop = step();
        if (stop) {
            if (*stop == X64Stop::halted) ++cpu_.instructions;
            if (*stop != X64Stop::stepLimit) { terminated_ = true; lastStop_ = *stop; }
            return result(*stop);
        }
        ++cpu_.instructions;
    }
    return result(X64Stop::stepLimit);
}

std::optional<X64BackendDiagnostic> runX64BackendDiagnostic() {
    GuestMemory memory;
    // Main executable: stack frame, multiply 6*7, CMP, conditional branch,
    // indirect CALL through guest import, restore frame, HLT.
    const std::uint8_t code[] = {
        0x55,                              // push rbp
        0x48, 0x89, 0xe5,                  // mov rbp,rsp
        0x48, 0xb8, 6, 0, 0, 0, 0, 0, 0, 0, // mov rax,6
        0x48, 0xbb, 7, 0, 0, 0, 0, 0, 0, 0, // mov rbx,7
        0x48, 0x0f, 0xaf, 0xc3,            // imul rax,rbx
        0x48, 0x3d, 42, 0, 0, 0,           // cmp rax,42
        0x75, 0x0c,                        // jne fail, skip call/frame cleanup/hlt
        0x48, 0xff, 0x15, 0, 0, 0, 0,     // call [rip + disp32], patched below
        0x48, 0x89, 0xec,                  // mov rsp,rbp
        0x5d,                              // pop rbp
        0xf4,                              // hlt
        0xcc                               // failure: unsupported opcode trap
    };
    std::vector<std::uint8_t> guest(code, code + sizeof(code));
    constexpr std::uint64_t guestStart = 0x2000;
    constexpr std::uint64_t slot = 0x2100;
    // Indirect CALL starts at instruction offset 32 and ends at offset 39.
    std::size_t callOffset = 0;
    for (std::size_t index = 0; index + 2 < guest.size(); ++index) {
        if (guest[index] == 0x48 && guest[index + 1] == 0xff && guest[index + 2] == 0x15) {
            callOffset = index;
            break;
        }
    }
    const auto afterCall = guestStart + callOffset + 7;
    const auto displacement = static_cast<std::uint32_t>(slot - afterCall);
    for (unsigned i = 0; i < 4; ++i)
        guest[callOffset + 3 + i] = static_cast<std::uint8_t>(displacement >> (8 * i));
    constexpr std::uint8_t libraryCode[] = {
        0x48, 0x83, 0xc0, 0x02, // add rax,2
        0xc3                    // ret
    };
    if (!memory.map(guestStart, guest.size(), permission::read | permission::write) ||
        !memory.writeBytes(guestStart, guest.data(), guest.size()) ||
        !memory.protect(guestStart, guest.size(), permission::read | permission::execute) ||
        !memory.map(slot, 8, permission::read | permission::write) ||
        !memory.map(0x3000, sizeof(libraryCode), permission::read | permission::write) ||
        !memory.writeBytes(0x3000, libraryCode, sizeof(libraryCode)) ||
        !memory.protect(0x3000, sizeof(libraryCode), permission::read | permission::execute))
        return std::nullopt;
    ModuleRegistry registry;
    if (!registry.registerModule({"libMisakiBackendTest", {{"increment2", 0x3000}}}) ||
        !registry.bindImport(memory, slot, "libMisakiBackendTest", "increment2") ||
        !memory.protect(slot, 8, permission::read)) return std::nullopt;
    const auto linked = memory.read64(slot);
    if (!linked) return std::nullopt;
    const bool readOnly = !memory.write64(slot, 0);
    PortableX64Backend backend(std::move(memory), guestStart);
    X64BackendDiagnostic report;
    report.execution = backend.run(60);
    report.importedSymbols = 1;
    report.importReadOnly = readOnly;
    report.linkedAddress = *linked;
    return report;
}

} // namespace misaki
