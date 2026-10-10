#include "../Include/GuestPM4.hpp"

#include <limits>
#include <utility>

namespace misaki {
namespace {
constexpr std::size_t maxWords = 4096;
constexpr std::size_t maxPackets = 512;
constexpr std::size_t maxRegisterWrites = 256;
constexpr std::size_t maxPacketBody = 1024;
constexpr std::uint64_t fnvOffset = 14695981039346656037ULL;
constexpr std::uint64_t fnvPrime = 1099511628211ULL;

void fail(std::string *error, const char *reason) {
    if (error) *error = reason;
}
void digest(std::uint64_t &hash, std::uint32_t value) {
    for (unsigned shift = 0; shift < 32; shift += 8) {
        hash ^= static_cast<std::uint8_t>(value >> shift);
        hash *= fnvPrime;
    }
}
struct RegisterBank {
    std::uint32_t base = 0;
    std::uint32_t span = 0;
    enum class Kind { context, shader, other } kind = Kind::other;
};
std::optional<RegisterBank> bankFor(std::uint8_t opcode) {
    using Kind = RegisterBank::Kind;
    switch (opcode) {
    case pm4_opcode::setConfigReg: return RegisterBank{0x2000, 0xC00, Kind::other};
    case pm4_opcode::setContextReg: return RegisterBank{0xA000, 0x400, Kind::context};
    case pm4_opcode::setShReg: return RegisterBank{0x2C00, 0x400, Kind::shader};
    case pm4_opcode::setUconfigReg: return RegisterBank{0xC000, 0x400, Kind::other};
    default: return std::nullopt;
    }
}
bool appendRegisters(PM4Trace &out, std::uint32_t base, std::uint32_t offset,
                     std::uint32_t maxOffset, const std::uint32_t *values,
                     std::size_t length, std::uint32_t shaderType,
                     RegisterBank::Kind kind) {
    if (length == 0 || length > maxRegisterWrites ||
        out.registers.size() > maxRegisterWrites - length ||
        offset >= maxOffset || length > maxOffset - offset) return false;
    for (std::size_t i = 0; i < length; ++i)
        out.registers.push_back({base + offset + static_cast<std::uint32_t>(i),
                                 values[i],
                                 static_cast<std::uint32_t>(out.packets.size()),
                                 shaderType});
    switch (kind) {
    case RegisterBank::Kind::context: out.contextWrites += static_cast<std::uint32_t>(length); break;
    case RegisterBank::Kind::shader: out.shaderWrites += static_cast<std::uint32_t>(length); break;
    case RegisterBank::Kind::other: out.otherWrites += static_cast<std::uint32_t>(length); break;
    }
    return true;
}
} // namespace

std::optional<PM4Trace> decodePM4(const std::uint32_t *words, std::size_t count,
                                  std::string *error) {
    if (!words || count == 0 || count > maxWords) {
        fail(error, "Empty or oversized PM4 stream");
        return std::nullopt;
    }
    PM4Trace out;
    out.checksum = fnvOffset;
    std::size_t at = 0;
    while (at < count) {
        if (out.packets.size() >= maxPackets) {
            fail(error, "Too many PM4 packets");
            return std::nullopt;
        }
        const std::uint32_t header = words[at];
        const std::uint32_t type = header >> 30;
        const std::size_t body = type == 2 ? 0 :
            static_cast<std::size_t>((header >> 16) & 0x3FFFu) + 1;
        if (body > maxPacketBody || body > count - at - 1) {
            fail(error, "Truncated or oversized PM4 packet body");
            return std::nullopt;
        }
        const auto *data = words + at + 1;
        std::uint32_t opcode = 0;
        switch (type) {
        case 0: { // PACKET0: N consecutive register writes, base in low 16 bits.
            const auto offset = header & 0xFFFFu;
            if (!appendRegisters(out, 0, offset, 0x10000, data, body, 0,
                                 RegisterBank::Kind::other)) {
                fail(error, "Invalid PACKET0 register range or budget");
                return std::nullopt;
            }
            ++out.type0Packets;
            break;
        }
        case 1:
            fail(error, "PACKET1 is not defined for this GCN profile");
            return std::nullopt;
        case 2:
            if (header != 0x80000000u) {
                fail(error, "Noncanonical PACKET2 padding");
                return std::nullopt;
            }
            ++out.type2Packets;
            break;
        case 3: {
            if (header & 1u) {
                fail(error, "Predicated PACKET3 not supported; would require condition state");
                return std::nullopt;
            }
            const auto shaderType = (header >> 1) & 1u;
            opcode = (header >> 8) & 0xFFu;
            const auto bank = bankFor(static_cast<std::uint8_t>(opcode));
            if (bank) {
                if (body < 2 || (data[0] & 0xFFFF0000u) != 0 ||
                    !appendRegisters(out, bank->base, data[0], bank->span,
                                     data + 1, body - 1, shaderType, bank->kind)) {
                    fail(error, "Malformed SET_*_REG packet or unsupported indexed mode");
                    return std::nullopt;
                }
            } else {
                switch (opcode) {
                case pm4_opcode::nop: ++out.nops; break; // payload may be a marker.
                case pm4_opcode::drawIndexAuto:
                    if (body != 2 || data[0] == 0 || data[0] > 1'000'000) {
                        fail(error, "Invalid DRAW_INDEX_AUTO payload");
                        return std::nullopt;
                    }
                    ++out.drawAuto;
                    out.lastVertexCount = data[0];
                    break;
                case pm4_opcode::eventWrite:
                    if (body != 1 && body != 3) {
                        fail(error, "Unexpected EVENT_WRITE payload length");
                        return std::nullopt;
                    }
                    ++out.eventWrites;
                    break;
                case pm4_opcode::indexType:
                    if (body != 1) { fail(error, "Invalid INDEX_TYPE payload"); return std::nullopt; }
                    break;
                case pm4_opcode::numInstances:
                    if (body != 1 || data[0] == 0) {
                        fail(error, "Invalid NUM_INSTANCES payload");
                        return std::nullopt;
                    }
                    break;
                default:
                    fail(error, "Unhandled PACKET3 opcode (not silently executed)");
                    return std::nullopt;
                }
            }
            ++out.type3Packets;
            break;
        }
        default:
            fail(error, "Invalid PM4 packet type");
            return std::nullopt;
        }
        out.packets.push_back({type, opcode, static_cast<std::uint32_t>(at),
                               static_cast<std::uint32_t>(body)});
        for (std::size_t i = 0; i <= body; ++i) digest(out.checksum, words[at + i]);
        at += body + 1;
    }
    return out;
}

std::optional<PM4Trace> decodeGuestPM4(const GuestMemory &memory,
                                       std::uint64_t address,
                                       std::size_t count, std::string *error) {
    if (count == 0 || count > maxWords ||
        address > std::numeric_limits<std::uint64_t>::max() - (count * 4 - 1)) {
        fail(error, "Bad guest command-buffer range");
        return std::nullopt;
    }
    std::vector<std::uint32_t> words;
    words.reserve(count);
    for (std::size_t i = 0; i < count; ++i) {
        std::uint32_t value = 0;
        for (unsigned byte = 0; byte < 4; ++byte) {
            const auto b = memory.read8(address + i * 4 + byte);
            if (!b) {
                fail(error, "Guest command-buffer read permission fault");
                return std::nullopt;
            }
            value |= static_cast<std::uint32_t>(*b) << (8 * byte);
        }
        words.push_back(value);
    }
    return decodePM4(words.data(), words.size(), error);
}

std::vector<std::uint32_t> makePM4DiagnosticStream() {
    const auto packet3 = [](std::uint32_t opcode, std::uint32_t bodyWords) {
        return 0xC0000000u | ((bodyWords - 1) << 16) | (opcode << 8);
    };
    return {
        0x80000000u, // type 2 padding
        packet3(pm4_opcode::nop, 1), 0x4D49534Bu, // arbitrary, self-authored NOP marker
        packet3(pm4_opcode::setContextReg, 3), 0x00B4, 0x10203040, 0x55667788,
        packet3(pm4_opcode::setShReg, 2), 0x000C, 0x00004000,
        packet3(pm4_opcode::setUconfigReg, 2), 0x0002, 0x00000001,
        packet3(pm4_opcode::indexType, 1), 0x00000000,
        packet3(pm4_opcode::numInstances, 1), 0x00000001,
        packet3(pm4_opcode::drawIndexAuto, 2), 3, 0,
        packet3(pm4_opcode::eventWrite, 1), 0x16,
    };
}
} // namespace misaki
