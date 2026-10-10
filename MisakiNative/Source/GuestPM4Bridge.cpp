#include "../Include/MisakiPM4Bridge.h"
#include "../Include/GuestPM4.hpp"

#include <cstdint>
#include <vector>

namespace {
int32_t exportTrace(const misaki::PM4Trace &trace,
                    MisakiPM4RegisterWrite *writes, size_t capacity,
                    MisakiPM4TraceReport *report, bool guestVerified) {
    if (capacity < trace.registers.size()) return -3;
    // All pointer and capacity checks are completed before writing anything.
    for (std::size_t i = 0; i < trace.registers.size(); ++i) {
        const auto &w = trace.registers[i];
        writes[i] = {w.address, w.value, w.packetIndex, w.shaderType};
    }
    report->abi_version = 1;
    report->packet_count = static_cast<uint32_t>(trace.packets.size());
    report->type0_packets = trace.type0Packets;
    report->type2_packets = trace.type2Packets;
    report->type3_packets = trace.type3Packets;
    report->nop_packets = trace.nops;
    report->register_writes = static_cast<uint32_t>(trace.registers.size());
    report->context_writes = trace.contextWrites;
    report->shader_writes = trace.shaderWrites;
    report->other_writes = trace.otherWrites;
    report->draw_auto_packets = trace.drawAuto;
    report->event_writes = trace.eventWrites;
    report->last_vertex_count = trace.lastVertexCount;
    report->guest_memory_verified = guestVerified ? 1u : 0u;
    report->checksum = trace.checksum;
    return 0;
}
}

extern "C" int32_t misaki_pm4_decode_words(const uint32_t *words, size_t word_count,
                                            MisakiPM4RegisterWrite *writes, size_t capacity,
                                            MisakiPM4TraceReport *report) {
    if (!report || (!writes && capacity != 0)) return -1;
    *report = MisakiPM4TraceReport{};
    try {
        const auto trace = misaki::decodePM4(words, word_count);
        if (!trace) return -2;
        if (!writes && !trace->registers.empty()) return -3;
        return exportTrace(*trace, writes, capacity, report, false);
    } catch (...) { return -4; }
}

extern "C" int32_t misaki_pm4_demo_trace(MisakiPM4RegisterWrite *writes, size_t capacity,
                                          MisakiPM4TraceReport *report) {
    if (!report || (!writes && capacity != 0)) return -1;
    *report = MisakiPM4TraceReport{};
    try {
        const auto stream = misaki::makePM4DiagnosticStream();
        std::vector<uint8_t> bytes;
        bytes.reserve(stream.size() * 4);
        for (const auto word : stream)
            for (unsigned shift = 0; shift < 32; shift += 8)
                bytes.push_back(static_cast<uint8_t>(word >> shift));
        misaki::GuestMemory memory;
        constexpr uint64_t guestAddress = 0x40000;
        if (!memory.map(guestAddress, bytes.size(),
                        misaki::permission::read | misaki::permission::write) ||
            !memory.writeBytes(guestAddress, bytes.data(), bytes.size()) ||
            !memory.protect(guestAddress, bytes.size(), misaki::permission::read) ||
            memory.writeBytes(guestAddress, bytes.data(), 1)) return -2;
        const auto trace = misaki::decodeGuestPM4(memory, guestAddress, stream.size());
        if (!trace) return -2;
        if (!writes && !trace->registers.empty()) return -3;
        return exportTrace(*trace, writes, capacity, report, true);
    } catch (...) { return -4; }
}
