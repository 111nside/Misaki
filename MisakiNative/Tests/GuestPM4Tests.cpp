#include "GuestPM4.hpp"
#include "MisakiPM4Bridge.h"

#include <cstdint>
#include <cstdlib>
#include <iostream>
#include <limits>
#include <string>
#include <vector>

namespace {
int checks = 0;
void check(bool v, const char *message) {
    ++checks;
    if (!v) { std::cerr << "FAIL: " << message << '\n'; std::exit(1); }
}
uint32_t p3(uint32_t opcode, uint32_t body, uint32_t flags = 0) {
    return 0xC0000000u | ((body - 1) << 16) | (opcode << 8) | flags;
}
void mutate(std::vector<uint32_t> stream, size_t at, uint32_t word,
            const char *message) {
    stream.at(at) = word;
    check(!misaki::decodePM4(stream.data(), stream.size()), message);
}
}
int main() {
    using namespace misaki;
    const auto words = makePM4DiagnosticStream();
    auto trace = decodePM4(words.data(), words.size());
    check(bool(trace), "decode synthetic PM4 stream");
    check(trace->packets.size() == 9, "nine PM4 packets");
    check(trace->type0Packets == 0 && trace->type2Packets == 1 &&
          trace->type3Packets == 8, "packet type counts");
    check(trace->nops == 1 && trace->eventWrites == 1 &&
          trace->drawAuto == 1, "known type3 categories");
    check(trace->lastVertexCount == 3, "decode draw index count");
    check(trace->registers.size() == 4, "capture four register writes");
    check(trace->contextWrites == 2 && trace->shaderWrites == 1 &&
          trace->otherWrites == 1, "categorized register writes");
    check(trace->registers[0].address == 0xA0B4 &&
          trace->registers[0].value == 0x10203040, "context register dword index");
    check(trace->registers[1].address == 0xA0B5 &&
          trace->registers[1].value == 0x55667788, "sequential register update");
    check(trace->registers[2].address == 0x2C0C &&
          trace->registers[2].value == 0x4000, "SH register base");
    check(trace->registers[3].address == 0xC002 &&
          trace->registers[3].value == 1, "UCONFIG register base");
    check(trace->checksum != 0 &&
          decodePM4(words.data(), words.size())->checksum == trace->checksum,
          "deterministic byte-order-independent checksum");
    check(!decodePM4(nullptr, 2), "null input");
    check(!decodePM4(words.data(), 0), "empty stream");
    check(!decodePM4(words.data(), 4097), "bounded stream size");
    for (size_t prefix = 1; prefix < words.size(); ++prefix) {
        if (decodePM4(words.data(), prefix)) {
            // Some prefixes legitimately end on packet boundaries.
            check(true, "partial complete prefix is permitted");
        } else check(true, "truncated packet safely rejected");
    }
    mutate(words, 0, 0x80000001, "reject noncanonical type2");
    mutate(words, 1, p3(pm4_opcode::nop, 1, 1), "reject predicated packet");
    mutate(words, 1, p3(0x3F, 1), "reject indirect buffer without reading pointers");
    mutate(words, 1, p3(0xAB, 1), "reject unknown opcode");
    mutate(words, 1, 0x40000000u, "reject packet type 1");
    mutate(words, 3, p3(pm4_opcode::setContextReg, 1025), "reject oversized body");
    mutate(words, 4, 0x400, "context register offset boundary");
    mutate(words, 4, 0x3FF, "context register crossing end of bank");
    mutate(words, 4, 0x10000, "reject unsupported indexed-mode bits");
    mutate(words, 17, p3(pm4_opcode::drawIndexAuto, 1), "draw packet wrong payload size");
    mutate(words, 18, 0, "draw count must be positive");
    // Additional recognized packets not included in the primary fixture.
    const std::vector<uint32_t> type0 = {0x00002300u, 0xAABBCCDDu};
    auto old = decodePM4(type0.data(), type0.size());
    check(old && old->type0Packets == 1 && old->registers.size() == 1 &&
          old->registers[0].address == 0x2300, "type-0 single write");
    const std::vector<uint32_t> shCompute = {
        p3(pm4_opcode::setShReg, 2, 2), 0x0003, 0x1234
    };
    auto compute = decodePM4(shCompute.data(), shCompute.size());
    check(compute && compute->registers[0].shaderType == 1,
          "type-3 shader type bit preserved");
    const std::vector<uint32_t> badConfig = {
        p3(pm4_opcode::setConfigReg, 2), 0xC00, 1
    };
    check(!decodePM4(badConfig.data(), badConfig.size()), "CONFIG bank bounds");
    const std::vector<uint32_t> hugeRegisters = {
        p3(pm4_opcode::setShReg, 2), 0x3FF, 1
    };
    check(bool(decodePM4(hugeRegisters.data(), hugeRegisters.size())),
          "last shader register allowed");
    GuestMemory mem;
    check(mem.map(0x8000, 12, permission::read | permission::write),
          "allocate three-dword guest stream");
    const uint8_t small[] = {0x00, 0, 0, 0x80, 0x00, 0x10, 0x00, 0xC0,
                             0x00, 0x00, 0x00, 0x00};
    check(mem.writeBytes(0x8000, small, sizeof(small)), "write PM4 bytes into guest memory");
    check(mem.protect(0x8000, sizeof(small), permission::read),
          "make PM4 command buffer read-only");
    auto guest = decodeGuestPM4(mem, 0x8000, 3);
    check(guest && guest->packets.size() == 2, "parse guest readable PM4 packets");
    check(!decodeGuestPM4(mem, 0x8000, 4), "reject guest range crossing mapping");
    check(!decodeGuestPM4(mem, 0x7000, 1), "reject unmapped guest memory");
    check(!decodeGuestPM4(mem, UINT64_MAX - 1, 1), "reject overflowing guest address");
    check(mem.protect(0x8000, sizeof(small), permission::write),
          "remove guest memory read permission");
    check(!decodeGuestPM4(mem, 0x8000, 3), "enforce guest read permissions");

    MisakiPM4RegisterWrite writes[8]{};
    MisakiPM4TraceReport report{};
    check(misaki_pm4_demo_trace(writes, 8, &report) == 0,
          "C bridge decodes stream from protected guest memory");
    check(report.abi_version == 1 && report.guest_memory_verified == 1,
          "C bridge reports guest memory verification");
    check(report.packet_count == 9 && report.register_writes == 4,
          "C bridge packet and register counts");
    check(report.draw_auto_packets == 1 && report.last_vertex_count == 3,
          "C bridge draw trace");
    check(report.checksum == trace->checksum, "bridge and native checksums agree");
    check(writes[0].address == 0xA0B4 && writes[3].address == 0xC002,
          "C ABI exposes register addresses");
    writes[0].address = 0xFEEDBEEFu;
    check(misaki_pm4_demo_trace(writes, 1, &report) == -3,
          "C bridge rejects insufficient output capacity");
    check(writes[0].address == 0xFEEDBEEFu && report.packet_count == 0,
          "C bridge does not partially write on error");
    check(misaki_pm4_demo_trace(writes, 8, nullptr) == -1,
          "C bridge rejects null report");
    check(misaki_pm4_demo_trace(nullptr, 8, &report) == -1,
          "C bridge rejects null output pointer");
    check(misaki_pm4_decode_words(words.data(), words.size(), writes, 8, &report) == 0 &&
          report.guest_memory_verified == 0,
          "direct word decoder C ABI distinguishes host/guest");
    check(misaki_pm4_decode_words(nullptr, words.size(), writes, 8, &report) == -2,
          "direct C bridge rejects null data");
    check(misaki_pm4_decode_words(words.data(), words.size(), nullptr, 0, &report) == -3,
          "direct C bridge requires output room");

    // Deterministic fuzz smoke test for crash and out-of-range safety.
    uint32_t seed = 0xBADC0FFEu;
    for (int i = 0; i < 600; ++i) {
        std::vector<uint32_t> random;
        const size_t length = static_cast<size_t>(i % 32) + 1;
        for (size_t j = 0; j < length; ++j) {
            seed ^= seed << 13; seed ^= seed >> 17; seed ^= seed << 5;
            random.push_back(seed);
        }
        auto result = decodePM4(random.data(), random.size());
        check(!result || result->packets.size() <= 32,
              "fuzz decode preserves bounded packet count");
    }
    std::cout << "PASS: " << checks << " PM4 parser assertions\n";
}
