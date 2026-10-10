#include "../Include/GuestPM4Indirect.hpp"

#include <algorithm>
#include <limits>
#include <utility>

namespace misaki {
namespace {
constexpr std::size_t maxPerBufferWords = 4096;
constexpr std::size_t maxTotalWords = 8192;
constexpr std::size_t maxPackets = 512;
constexpr std::size_t maxRegisters = 256;
constexpr std::uint32_t maxDepth = 3; // root=0; child=1; grandchild=2
constexpr std::uint64_t fnvOffset = 14695981039346656037ULL;
constexpr std::uint64_t fnvPrime = 1099511628211ULL;

void fail(std::string *error, const char *message) {
    if (error) *error = message;
}
void digest(std::uint64_t &hash, std::uint32_t word) {
    for (unsigned shift = 0; shift < 32; shift += 8) {
        hash ^= static_cast<std::uint8_t>(word >> shift);
        hash *= fnvPrime;
    }
}
std::uint32_t packet3(std::uint8_t opcode, std::uint32_t bodyCount) {
    return 0xC0000000u | ((bodyCount - 1u) << 16) | (std::uint32_t(opcode) << 8);
}
std::vector<std::uint8_t> toBytes(const std::vector<std::uint32_t> &words) {
    std::vector<std::uint8_t> bytes;
    bytes.reserve(words.size() * 4);
    for (const auto word : words)
        for (unsigned n = 0; n < 4; ++n)
            bytes.push_back(static_cast<std::uint8_t>(word >> (n * 8)));
    return bytes;
}
bool mapWords(GuestMemory &memory, std::uint64_t address,
              const std::vector<std::uint32_t> &words) {
    if (words.empty()) return false;
    const auto bytes = toBytes(words);
    return memory.map(address, bytes.size(), permission::read | permission::write) &&
           memory.writeBytes(address, bytes.data(), bytes.size()) &&
           memory.protect(address, bytes.size(), permission::read);
}

class Decoder {
public:
    Decoder(const GuestMemory &memory, std::string *error)
        : memory_(memory), error_(error) { trace_.checksum = fnvOffset; }

    std::optional<PM4IndirectTrace> run(std::uint64_t address, std::size_t count) {
        if (!decode(address, count, 0)) return std::nullopt;
        return trace_;
    }

private:
    const GuestMemory &memory_;
    std::string *error_;
    PM4IndirectTrace trace_;
    std::vector<std::uint64_t> activeBuffers_;

    bool reject(const char *message) {
        fail(error_, message);
        return false;
    }
    bool readBuffer(std::uint64_t address, std::size_t count,
                    std::vector<std::uint32_t> &words) {
        if ((address & 3u) != 0 || count == 0 || count > maxPerBufferWords ||
            count > maxTotalWords - trace_.totalWords ||
            address > std::numeric_limits<std::uint64_t>::max() - (count * 4 - 1))
            return reject("Unaligned, oversized, or overflowing indirect buffer");
        words.reserve(count);
        for (std::size_t i = 0; i < count; ++i) {
            std::uint32_t value = 0;
            for (unsigned n = 0; n < 4; ++n) {
                const auto byte = memory_.read8(address + 4 * i + n);
                if (!byte) return reject("Unmapped or unreadable guest PM4 buffer");
                value |= std::uint32_t(*byte) << (8 * n);
            }
            words.push_back(value);
        }
        trace_.totalWords += static_cast<std::uint32_t>(count);
        return true;
    }
    void recordRegister(const PM4RegisterWrite &write) {
        auto &state = trace_.state;
        switch (write.address) {
        case 0xA318: state.color0Base = write.value; state.observedMask |= 1u; break;
        case 0xA319: state.color0Pitch = write.value; state.observedMask |= 2u; break;
        case 0xA31C: state.color0Info = write.value; state.observedMask |= 4u; break;
        case 0xA08E: state.targetMask = write.value; state.observedMask |= 8u; break;
        case 0xA080: state.scissorTL = write.value; state.observedMask |= 16u; break;
        case 0xA081: state.scissorBR = write.value; state.observedMask |= 32u; break;
        case 0x2C08: state.pixelShaderLow = write.value; state.observedMask |= 64u; break;
        case 0x2C09: state.pixelShaderHigh = write.value; state.observedMask |= 128u; break;
        default: break;
        }
    }
    bool decode(std::uint64_t address, std::size_t count, std::uint32_t depth) {
        if (depth > maxDepth) return reject("Indirect PM4 nesting depth exceeded");
        if (std::find(activeBuffers_.begin(), activeBuffers_.end(), address) !=
            activeBuffers_.end()) return reject("Indirect PM4 cycle detected");
        activeBuffers_.push_back(address);
        std::vector<std::uint32_t> words;
        if (!readBuffer(address, count, words)) return false;
        trace_.maxDepth = std::max(trace_.maxDepth, depth);
        std::size_t at = 0;
        while (at < words.size()) {
            if (trace_.packets.size() >= maxPackets)
                return reject("Global PM4 packet limit exceeded");
            const std::uint32_t header = words[at];
            const std::uint32_t type = header >> 30;
            const std::size_t body = type == 2 ? 0 :
                static_cast<std::size_t>((header >> 16) & 0x3FFFu) + 1;
            if (body > words.size() - at - 1 || body > 1024)
                return reject("Truncated or oversized nested PM4 packet");
            const std::uint32_t opcode = type == 3 ? ((header >> 8) & 0xFFu) : 0;
            const auto packetIndex = static_cast<std::uint32_t>(trace_.packets.size());
            const bool indirect = type == 3 &&
                (opcode == pm4_indirect_opcode::indirectBuffer ||
                 opcode == pm4_indirect_opcode::indirectBufferConst);
            if (indirect) {
                if ((header & 3u) != 0 || body != 3)
                    return reject("Unsupported predicated/compute indirect packet or body size");
                const auto *data = words.data() + at + 1;
                // Control contains a 20-bit number of DWORDs. Higher flags such as
                // chained execution are intentionally unsupported and rejected.
                if ((data[2] & 0xFFF00000u) != 0)
                    return reject("Unsupported indirect-buffer control flags");
                const std::uint32_t childCount = data[2] & 0xFFFFFu;
                if (childCount == 0)
                    return reject("Zero-length indirect buffer");
                const std::uint64_t childAddress =
                    (std::uint64_t(data[1]) << 32) | data[0];
                if ((childAddress & 3u) != 0 || depth >= maxDepth)
                    return reject("Unaligned indirect pointer or nesting depth exceeded");
                trace_.packets.push_back({address + at * 4, type, opcode, depth,
                                          static_cast<std::uint32_t>(body)});
                ++trace_.indirectBuffers;
                if (opcode == pm4_indirect_opcode::indirectBufferConst)
                    ++trace_.indirectConstBuffers;
                for (std::size_t j = 0; j <= body; ++j)
                    digest(trace_.checksum, words[at + j]);
                // The child is decoded at its position in the parent command
                // stream, preserving ordering for the register-state snapshot.
                if (!decode(childAddress, childCount, depth + 1)) return false;
            } else {
                // Delegate every non-indirect packet to the Milestone 22
                // decoder: no new host-side GPU commands are executed.
                const auto one = decodePM4(words.data() + at, body + 1);
                if (!one || one->packets.size() != 1)
                    return reject("Invalid or unsupported PM4 packet in indirect stream");
                trace_.packets.push_back({address + at * 4, type, opcode, depth,
                                          static_cast<std::uint32_t>(body)});
                if (one->registers.size() > maxRegisters - trace_.registers.size())
                    return reject("Global PM4 register-write limit exceeded");
                for (auto write : one->registers) {
                    write.packetIndex = packetIndex;
                    trace_.registers.push_back(write);
                    recordRegister(write);
                }
                trace_.registerWrites += static_cast<std::uint32_t>(one->registers.size());
                trace_.drawAuto += one->drawAuto;
                trace_.eventWrites += one->eventWrites;
                for (std::size_t j = 0; j <= body; ++j)
                    digest(trace_.checksum, words[at + j]);
            }
            at += body + 1;
        }
        activeBuffers_.pop_back();
        return true;
    }
};
} // namespace

std::optional<PM4IndirectTrace> decodePM4Indirect(const GuestMemory &memory,
                                                   std::uint64_t address,
                                                   std::size_t wordCount,
                                                   std::string *error) {
    Decoder decoder(memory, error);
    return decoder.run(address, wordCount);
}

PM4IndirectFixture makePM4IndirectFixture() {
    PM4IndirectFixture result;
    // A root command stream invokes a secondary stream, which invokes a
    // third. All packets use authentic PM4 Type-3 header *layouts* and known
    // GCN opcode/register encodings, with invented test values.
    result.grandchildWords = {
        packet3(pm4_opcode::setContextReg, 3), 0x80, 0x000A000Au, 0x007800C8u,
        packet3(pm4_opcode::nop, 1), 0x4D49534Bu,
        packet3(pm4_opcode::setContextReg, 2), 0x8Eu, 0x0000000Fu
    };
    result.childWords = {
        packet3(pm4_opcode::setContextReg, 2), 0x31Cu, 0x0000001Bu,
        packet3(pm4_opcode::setShReg, 3), 0x08u, 0x00001000u, 0u,
        packet3(pm4_indirect_opcode::indirectBufferConst, 3),
            static_cast<std::uint32_t>(result.grandchild), 0u,
            static_cast<std::uint32_t>(result.grandchildWords.size()),
        packet3(pm4_opcode::drawIndexAuto, 2), 3u, 0u
    };
    result.rootWords = {
        0x80000000u,
        packet3(pm4_opcode::setContextReg, 3), 0x318u, 0x00200000u, 127u,
        packet3(pm4_indirect_opcode::indirectBuffer, 3),
            static_cast<std::uint32_t>(result.child), 0u,
            static_cast<std::uint32_t>(result.childWords.size()),
        packet3(pm4_opcode::eventWrite, 1), 0x16u
    };
    return result;
}

bool installPM4IndirectFixture(GuestMemory &memory,
                               const PM4IndirectFixture &fixture) {
    // Transactional mappings: if any entry fails, caller's memory is intact.
    GuestMemory candidate = memory;
    if (!mapWords(candidate, fixture.root, fixture.rootWords) ||
        !mapWords(candidate, fixture.child, fixture.childWords) ||
        !mapWords(candidate, fixture.grandchild, fixture.grandchildWords))
        return false;
    memory = std::move(candidate);
    return true;
}
} // namespace misaki
