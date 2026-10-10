#include "GuestPM4Indirect.hpp"
#include "MisakiPM423Bridge.h"

#include <cstdint>
#include <cstdlib>
#include <iostream>
#include <limits>
#include <string>
#include <vector>

namespace {
int checks = 0;
void check(bool ok, const char *message) {
    ++checks;
    if (!ok) { std::cerr << "FAIL: " << message << '\n'; std::exit(EXIT_FAILURE); }
}
std::optional<misaki::PM4IndirectTrace> runFixture(
    const misaki::PM4IndirectFixture &fixture) {
    misaki::GuestMemory memory;
    if (!misaki::installPM4IndirectFixture(memory, fixture)) return std::nullopt;
    return misaki::decodePM4Indirect(memory, fixture.root, fixture.rootWords.size());
}
void reject(misaki::PM4IndirectFixture fixture, const char *message) {
    check(!runFixture(fixture), message);
}
}

int main() {
    using namespace misaki;
    auto fixture = makePM4IndirectFixture();
    GuestMemory memory;
    check(installPM4IndirectFixture(memory, fixture), "map bounded read-only fixture buffers");
    check(!memory.write64(fixture.root, 0), "root buffer read-only after installation");
    check(!memory.write64(fixture.child, 0), "child buffer read-only after installation");
    check(!memory.write64(fixture.grandchild, 0), "grandchild buffer read-only");
    std::string error;
    const auto run = decodePM4Indirect(memory, fixture.root, fixture.rootWords.size(), &error);
    check(bool(run), "decode recursive authentic-form PM4 packets");
    check(error.empty(), "successful decode does not set error");
    check(run->packets.size() == 11, "11 packets in execution order");
    check(run->indirectBuffers == 2 && run->indirectConstBuffers == 1,
          "regular and const indirect buffers counted");
    check(run->maxDepth == 2, "two levels of nesting");
    check(run->totalWords == 34, "cumulative root child and grandchild words");
    check(run->registerWrites == 8 && run->registers.size() == 8,
          "eight ordered register writes");
    check(run->drawAuto == 1 && run->eventWrites == 1, "draw and event preserved");
    check(run->checksum != 0, "nonzero deterministic ordered-stream checksum");
    check(run->packets[0].depth == 0 && run->packets[0].type == 2,
          "root packet2 preserved");
    check(run->packets[2].opcode == pm4_indirect_opcode::indirectBuffer &&
          run->packets[2].depth == 0, "root indirect packet");
    check(run->packets[3].depth == 1 && run->packets[3].opcode == pm4_opcode::setContextReg,
          "first child packet decoded immediately after parent IB");
    check(run->packets[5].opcode == pm4_indirect_opcode::indirectBufferConst &&
          run->packets[5].depth == 1, "const indirect packet recorded");
    check(run->packets[6].depth == 2 && run->packets[7].depth == 2 &&
          run->packets[8].depth == 2, "grandchild execution order");
    check(run->packets[9].depth == 1 && run->packets[9].opcode == pm4_opcode::drawIndexAuto,
          "child execution resumes after grandchild");
    check(run->packets[10].depth == 0 && run->packets[10].opcode == pm4_opcode::eventWrite,
          "root resumes after child");
    check(run->packets[2].guestAddress == fixture.root + 20,
          "root PM4 header provenance preserved");
    check(run->packets[5].guestAddress == fixture.child + 28,
          "child PM4 header provenance preserved");
    check(run->packets[6].guestAddress == fixture.grandchild,
          "grandchild guest address preserved");
    check(run->registers[0].address == 0xA318 &&
          run->registers[0].packetIndex == 1, "PM4 register and global index");
    check(run->registers[2].address == 0xA31C &&
          run->registers[2].packetIndex == 3, "child register and global index");
    check(run->registers[5].address == 0xA080 &&
          run->registers[5].packetIndex == 6, "grandchild register and global index");
    check(run->state.observedMask == 255, "all eight tracked registers observed");
    check(run->state.hasRenderTargetMetadata(), "raw RT metadata observed");
    check(run->state.color0Base == 0x200000 && run->state.color0Pitch == 127 &&
          run->state.color0Info == 0x1B && run->state.targetMask == 15,
          "raw render target values in child-order execution");
    check(run->state.scissorTL == 0x000A000A &&
          run->state.scissorBR == 0x007800C8, "scissor raw values preserved");
    check(run->state.pixelShaderLow == 0x1000 && run->state.pixelShaderHigh == 0,
          "raw pixel shader registers preserved");
    check(runFixture(fixture)->checksum == run->checksum, "fixture deterministic checksum");

    auto mutated = fixture;
    mutated.rootWords[3] = 0x333333;
    auto changed = runFixture(mutated);
    check(changed && changed->state.color0Base == 0x333333 &&
          changed->checksum != run->checksum, "altered command changes snapshot/checksum");
    mutated = fixture;
    mutated.grandchildWords[8] = 0xF;
    check(runFixture(mutated)->checksum == run->checksum,
          "identical stream stays deterministic");
    mutated = fixture;
    mutated.rootWords[6] = 0xDEAD0000;
    reject(mutated, "reject unreadable child pointer");
    mutated = fixture;
    mutated.rootWords[6] |= 1;
    reject(mutated, "reject misaligned child pointer");
    mutated = fixture;
    mutated.rootWords[7] = 1;
    reject(mutated, "reject unmapped 64-bit child pointer");
    mutated = fixture;
    mutated.rootWords[8] = 0;
    reject(mutated, "reject zero-size indirect buffer");
    mutated = fixture;
    mutated.rootWords[8] |= 1u << 20;
    reject(mutated, "fail closed on chained indirect control flag");
    mutated = fixture;
    mutated.rootWords[8] = 4097;
    reject(mutated, "reject oversized indirect command buffer");
    mutated = fixture;
    mutated.rootWords[8] += 1;
    reject(mutated, "reject truncated indirect buffer");
    mutated = fixture;
    mutated.rootWords[5] |= 1;
    reject(mutated, "reject predicated indirect packet");
    mutated = fixture;
    mutated.rootWords[5] |= 2;
    reject(mutated, "reject compute-type indirect packet in subset");
    mutated = fixture;
    mutated.rootWords[5] = 0xC0003F00u; // body count 1 (not 3)
    reject(mutated, "reject malformed indirect header body length");
    mutated = fixture;
    mutated.rootWords[6] = static_cast<uint32_t>(fixture.root);
    mutated.rootWords[8] = static_cast<uint32_t>(fixture.rootWords.size());
    reject(mutated, "reject self-referential indirect buffer");
    mutated = fixture;
    mutated.childWords[8] = static_cast<uint32_t>(fixture.root);
    mutated.childWords[10] = static_cast<uint32_t>(fixture.rootWords.size());
    reject(mutated, "reject child to ancestor cycle");
    mutated = fixture;
    mutated.grandchildWords = {
        0xC0023F00u, static_cast<uint32_t>(fixture.root), 0,
        static_cast<uint32_t>(fixture.rootWords.size())
    };
    mutated.childWords[10] = static_cast<uint32_t>(mutated.grandchildWords.size());
    reject(mutated, "reject deep cycle through grandchild");
    mutated = fixture;
    mutated.grandchildWords = {
        0xC0023F00u, 0x70000u, 0u, 1u
    };
    mutated.childWords[10] = static_cast<uint32_t>(mutated.grandchildWords.size());
    reject(mutated, "reject unmapped fourth-level indirect pointer");
    mutated = fixture;
    mutated.childWords[11] = 0xC001AA00u;
    reject(mutated, "reject unknown child packet3 opcode");
    mutated = fixture;
    mutated.grandchildWords[0] = 0x40000000u;
    reject(mutated, "reject PM4 packet type1 in child");
    mutated = fixture;
    mutated.childWords[12] = 0;
    reject(mutated, "reject bad draw metadata in child");
    mutated = fixture;
    mutated.rootWords[2] = 0x400;
    reject(mutated, "reject register bank violation");
    mutated = fixture;
    mutated.rootWords[9] = 0xC0024600u;
    reject(mutated, "reject malformed parent event body");

    check(!decodePM4Indirect(memory, fixture.root, 0), "zero word count rejected");
    check(!decodePM4Indirect(memory, fixture.root + 1, 1), "unaligned guest address rejected");
    check(!decodePM4Indirect(memory, fixture.root, 4097), "buffer count over maximum rejected");
    check(!decodePM4Indirect(memory, UINT64_MAX - 1, 1), "address overflow rejected");
    check(!decodePM4Indirect(memory, 0x70000, 1), "unmapped root buffer rejected");
    GuestMemory noRead;
    check(noRead.map(0xB000, 4, permission::write), "map write-only command buffer");
    check(!decodePM4Indirect(noRead, 0xB000, 1), "reject missing read permission");
    GuestMemory collision;
    check(collision.map(fixture.child, 32, permission::read), "prepare collision memory");
    check(!installPM4IndirectFixture(collision, fixture), "transactional install rejects overlap");
    check(!collision.read8(fixture.root), "failed installation does not add root mapping");
    check(collision.read8(fixture.child) == 0, "failed installation preserves old mapping");

    MisakiPM423Report report{};
    MisakiPM423Packet packets[32]{};
    check(misaki_pm423_diagnostic(packets, 32, &report) == 0,
          "C ABI decodes guest PM4 indirect demo");
    check(report.abi_version == 1 && report.packet_count == 11 &&
          report.indirect_buffers == 2 && report.indirect_const_buffers == 1 &&
          report.maximum_depth == 2, "C ABI packet counts and depth");
    check(report.guest_words == 34 && report.register_writes == 8 &&
          report.draw_auto_packets == 1 && report.event_writes == 1,
          "C ABI overall PM4 summary");
    check(report.observed_register_mask == 255 &&
          report.render_target_metadata_observed == 1,
          "C ABI raw render target metadata flag");
    check(report.color0_base == 0x200000 && report.color0_pitch == 127 &&
          report.color0_info == 27 && report.color_target_mask == 15,
          "C ABI raw color registers");
    check(report.pixel_shader_low == 0x1000 && report.pixel_shader_high == 0,
          "C ABI raw shader registers");
    check(report.scissor_tl == 0x000A000A && report.scissor_br == 0x007800C8,
          "C ABI raw scissor registers");
    check(report.checksum == run->checksum, "C ABI and C++ checksum equal");
    check(packets[2].guest_address == fixture.root + 20 && packets[2].depth == 0,
          "C ABI preserves packet provenance");
    check(packets[6].guest_address == fixture.grandchild && packets[6].depth == 2,
          "C ABI preserves nested packet provenance");
    packets[0].opcode = 0xFEDCBA98u;
    check(misaki_pm423_diagnostic(packets, 4, &report) == -3,
          "C ABI rejects small output capacity");
    check(packets[0].opcode == 0xFEDCBA98u && report.packet_count == 0,
          "capacity failure does not partially overwrite output");
    check(misaki_pm423_diagnostic(nullptr, 0, &report) == -3,
          "null packet output with zero capacity is too small");
    check(misaki_pm423_diagnostic(nullptr, 1, &report) == -1,
          "null output with nonzero capacity rejected");
    check(misaki_pm423_diagnostic(packets, 32, nullptr) == -1,
          "null report rejected");

    // Deterministic fuzzing of packet bodies and indirect pointers.
    std::uint32_t seed = 0x8128fca5u;
    for (unsigned i = 0; i < 256; ++i) {
        auto candidate = fixture;
        seed ^= seed << 13; seed ^= seed >> 17; seed ^= seed << 5;
        const std::size_t index = seed % candidate.childWords.size();
        candidate.childWords[index] ^= seed;
        auto result = runFixture(candidate);
        check(!result || (result->packets.size() <= 512 &&
                          result->registers.size() <= 256 &&
                          result->totalWords <= 8192),
              "mutated indirect chain obeys global budget");
    }
    std::cout << "PASS: " << checks << " PM4 indirect/render-state assertions\n";
}
