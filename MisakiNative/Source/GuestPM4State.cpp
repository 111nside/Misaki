#include "../Include/GuestPM4State.hpp"

#include <limits>

namespace misaki {
namespace {
constexpr std::uint32_t required = 0xFFu; // eight registers from M23
constexpr std::uint32_t colorMask = 0xFu;

// The AMD PM4 register fields are inspected, not interpreted as a fully
// validated target descriptor. The numeric COLOR_FORMAT field is preserved.
PM4DrawSnapshot makeSnapshot(const PM4RawRenderState &s,
                             const PM4IndirectPacket &packet,
                             std::uint32_t index, std::uint32_t vertices) {
    PM4DrawSnapshot d;
    d.packetIndex = index;
    d.packetAddress = packet.guestAddress;
    d.depth = packet.depth;
    d.vertexCount = vertices;
    d.pitchTileMax = s.color0Pitch & 0x7FFu;
    d.colorFormatField = s.color0Info & 0x3Fu;
    d.targetMask = s.targetMask;
    d.observedMask = s.observedMask;
    // The PM4 registers give 256-byte units rather than host pointers.
    d.colorAddress = std::uint64_t(s.color0Base) << 8;
    // Shader high register contributes bits [47:40] for this bounded subset.
    d.pixelShaderAddress = (std::uint64_t(s.pixelShaderHigh & 0xFFu) << 40) |
                           (std::uint64_t(s.pixelShaderLow) << 8);
    const std::uint32_t left = s.scissorTL & 0x7FFFu;
    const std::uint32_t top = (s.scissorTL >> 16) & 0x7FFFu;
    const std::uint32_t right = s.scissorBR & 0x7FFFu;
    const std::uint32_t bottom = (s.scissorBR >> 16) & 0x7FFFu;
    d.scissorX = left;
    d.scissorY = top;
    // Avoid wraparound on backwards coordinates; classify below.
    if (right > left) d.scissorWidth = right - left;
    if (bottom > top) d.scissorHeight = bottom - top;
    if ((s.observedMask & required) != required)
        d.status = PM4DrawStatus::missingRegisters;
    else if ((s.targetMask & colorMask) == 0)
        d.status = PM4DrawStatus::disabledTarget;
    else if ((s.targetMask & ~colorMask) != 0)
        d.status = PM4DrawStatus::unsupportedTargets;
    else if (d.scissorWidth == 0 || d.scissorHeight == 0 ||
             (s.scissorTL & 0x80008000u) || (s.scissorBR & 0x80008000u))
        d.status = PM4DrawStatus::invalidScissor;
    else if (s.color0Base == 0 || s.pixelShaderLow == 0 ||
             (s.pixelShaderHigh & ~0xFFu) != 0)
        d.status = PM4DrawStatus::invalidAddress;
    else if (d.colorFormatField == 0)
        d.status = PM4DrawStatus::unsupportedFormat;
    else d.status = PM4DrawStatus::metadataReady;
    return d;
}

void applyState(PM4RawRenderState &s, const PM4RegisterWrite &write) {
    switch (write.address) {
    case 0xA318: s.color0Base = write.value; s.observedMask |= 1u; break;
    case 0xA319: s.color0Pitch = write.value; s.observedMask |= 2u; break;
    case 0xA31C: s.color0Info = write.value; s.observedMask |= 4u; break;
    case 0xA08E: s.targetMask = write.value; s.observedMask |= 8u; break;
    case 0xA080: s.scissorTL = write.value; s.observedMask |= 16u; break;
    case 0xA081: s.scissorBR = write.value; s.observedMask |= 32u; break;
    case 0x2C08: s.pixelShaderLow = write.value; s.observedMask |= 64u; break;
    case 0x2C09: s.pixelShaderHigh = write.value; s.observedMask |= 128u; break;
    default: break;
    }
}

std::optional<std::uint32_t> readWord(const GuestMemory &memory,
                                     std::uint64_t address) {
    if (address > std::numeric_limits<std::uint64_t>::max() - 3)
        return std::nullopt;
    std::uint32_t value = 0;
    for (unsigned i = 0; i < 4; ++i) {
        const auto byte = memory.read8(address + i);
        if (!byte) return std::nullopt;
        value |= std::uint32_t(*byte) << (8 * i);
    }
    return value;
}
} // namespace

std::optional<PM4StateTrace> analyzePM4DrawStates(
    const GuestMemory &memory, std::uint64_t address, std::size_t words,
    std::string *error) {
    auto trace = decodePM4Indirect(memory, address, words, error);
    if (!trace) return std::nullopt;
    PM4StateTrace result;
    result.packets = static_cast<std::uint32_t>(trace->packets.size());
    result.indirectBuffers = trace->indirectBuffers;
    result.registerWrites = trace->registerWrites;
    result.checksum = trace->checksum;
    PM4RawRenderState state;
    std::size_t nextWrite = 0;
    for (std::uint32_t i = 0; i < trace->packets.size(); ++i) {
        const auto &packet = trace->packets[i];
        while (nextWrite < trace->registers.size() &&
               trace->registers[nextWrite].packetIndex == i) {
            applyState(state, trace->registers[nextWrite]);
            ++nextWrite;
        }
        if (packet.type != 3 || packet.opcode != pm4_opcode::drawIndexAuto)
            continue;
        if (result.draws.size() >= 64 || packet.bodyWords != 2 ||
            packet.guestAddress > std::numeric_limits<std::uint64_t>::max() - 4) {
            if (error) *error = "Invalid draw snapshot bounds";
            return std::nullopt;
        }
        const auto vertices = readWord(memory, packet.guestAddress + 4);
        if (!vertices || *vertices == 0 || *vertices > 1'000'000) {
            if (error) *error = "Unreadable/invalid DRAW_INDEX_AUTO count";
            return std::nullopt;
        }
        auto draw = makeSnapshot(state, packet, i, *vertices);
        if (draw.status == PM4DrawStatus::metadataReady) ++result.readyDraws;
        else ++result.rejectedDraws;
        result.draws.push_back(draw);
    }
    if (nextWrite != trace->registers.size()) {
        if (error) *error = "Inconsistent register event ordering";
        return std::nullopt;
    }
    return result;
}

std::optional<PM4StateTrace> runPM4StateDiagnostic() {
    const auto f = makePM4IndirectFixture();
    GuestMemory memory;
    if (!installPM4IndirectFixture(memory, f)) return std::nullopt;
    return analyzePM4DrawStates(memory, f.root, f.rootWords.size());
}
} // namespace misaki
