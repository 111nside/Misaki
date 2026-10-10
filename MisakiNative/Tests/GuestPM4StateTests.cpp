#include "GuestPM4State.hpp"
#include "MisakiPM424Bridge.h"
#include <cstdlib>
#include <cstdint>
#include <iostream>
#include <vector>

namespace {
unsigned assertions = 0;
void check(bool condition, const char *what) {
    ++assertions;
    if (!condition) { std::cerr << "FAIL: " << what << '\n'; std::exit(1); }
}
uint32_t packet3(uint32_t opcode, uint32_t words) {
    return 0xC0000000u | ((words-1) << 16) | (opcode << 8);
}
std::optional<misaki::PM4StateTrace> run(misaki::PM4IndirectFixture f) {
    misaki::GuestMemory memory;
    if (!misaki::installPM4IndirectFixture(memory, f)) return std::nullopt;
    return misaki::analyzePM4DrawStates(memory, f.root, f.rootWords.size());
}
void rejectedState(misaki::PM4IndirectFixture fixture, uint32_t expected,
                   const char *message) {
    const auto trace = run(std::move(fixture));
    check(trace && trace->draws.size() == 1 &&
          static_cast<uint32_t>(trace->draws[0].status) == expected &&
          trace->rejectedDraws == 1 && trace->readyDraws == 0, message);
}
}

int main() {
    using namespace misaki;
    const auto fixture = makePM4IndirectFixture();
    const auto normal = run(fixture);
    check(bool(normal), "valid PM4 state fixture");
    check(normal->packets == 11 && normal->indirectBuffers == 2, "nested packet count");
    check(normal->registerWrites == 8, "register replay count");
    check(normal->draws.size() == 1 && normal->readyDraws == 1 &&
          normal->rejectedDraws == 0, "one coherent draw snapshot");
    const auto d = normal->draws.front();
    check(d.status == PM4DrawStatus::metadataReady, "metadata-ready, not drawable");
    check(d.depth == 1 && d.packetIndex == 9, "draw comes from child after grandchild");
    check(d.vertexCount == 3, "draw vertex payload");
    check(d.packetAddress == fixture.child + 11 * 4, "draw packet guest address");
    check(d.colorAddress == (uint64_t(0x200000) << 8), "color base scaled by 256");
    check(d.pixelShaderAddress == (uint64_t(0x1000) << 8), "shader base scaled by 256");
    check(d.scissorX == 10 && d.scissorY == 10, "scissor origin");
    check(d.scissorWidth == 190 && d.scissorHeight == 110, "scissor extent");
    check(d.pitchTileMax == 127 && d.colorFormatField == 27, "raw surface fields");
    check(d.targetMask == 15 && d.observedMask == 255, "target and complete bit mask");
    check(normal->checksum != 0, "checksum remains in PM4 trace");
    check(run(fixture)->checksum == normal->checksum, "deterministic checksums");

    auto bad = fixture;
    bad.grandchildWords[3] = 0x00050005u; // BR before TL: invalid rect
    rejectedState(bad, 3, "backwards scissor rejected");
    bad = fixture;
    bad.grandchildWords[8] = 0u; // target disabled
    rejectedState(bad, 2, "disabled render target detected");
    bad = fixture;
    bad.grandchildWords[8] = 0x1Fu; // extra targets
    rejectedState(bad, 5, "extra render targets not accepted");
    bad = fixture;
    bad.rootWords[3] = 0u; // zero color base
    rejectedState(bad, 4, "null target base not accepted");
    bad = fixture;
    bad.childWords[2] = 0u; // CB_COLOR0_INFO raw format field zero
    rejectedState(bad, 6, "undefined format field not accepted");
    bad = fixture;
    bad.childWords[5] = 0u; // missing valid shader base
    rejectedState(bad, 4, "null pixel shader pointer not accepted");
    bad = fixture;
    bad.childWords[6] = 0x100u; // high shader address reserved bits
    rejectedState(bad, 4, "unsupported shader address bits rejected");
    bad = fixture;
    bad.grandchildWords = {packet3(pm4_opcode::setContextReg, 2), 0x8E, 0xF};
    bad.childWords[10] = static_cast<uint32_t>(bad.grandchildWords.size());
    rejectedState(bad, 1, "missing scissor metadata detected");

    // Test two separate snapshots: second draw is preceded by a scissor update.
    auto multiple = fixture;
    multiple.childWords.insert(multiple.childWords.end(),
        {packet3(pm4_opcode::setContextReg, 2), 0x80, 0x00140014u,
         packet3(pm4_opcode::drawIndexAuto, 2), 5, 0});
    multiple.rootWords[8] = static_cast<uint32_t>(multiple.childWords.size());
    const auto twice = run(multiple);
    check(twice && twice->draws.size() == 2 && twice->readyDraws == 2,
          "two draws recorded independently");
    check(twice->draws[0].scissorX == 10 && twice->draws[1].scissorX == 20,
          "state changes between draws are replayed correctly");
    check(twice->draws[0].scissorWidth == 190 &&
          twice->draws[1].scissorWidth == 180, "new scissor applied only to second draw");
    check(twice->draws[0].vertexCount == 3 && twice->draws[1].vertexCount == 5,
          "distinct vertex counts");
    check(twice->draws[1].packetIndex > twice->draws[0].packetIndex,
          "ordered packet indexes");
    check(twice->checksum != normal->checksum, "different command stream checksum");

    bad = fixture;
    bad.rootWords[8] = 0xFFFFFu; // illegal child size: parse fails closed
    check(!run(bad), "nested malformed data rejected by original PM4 decoder");
    GuestMemory missing;
    check(!analyzePM4DrawStates(missing, fixture.root, fixture.rootWords.size()),
          "unmapped guest memory rejected");
    check(!analyzePM4DrawStates(missing, 3, 1), "unaligned address rejected");

    MisakiPM424Draw rows[2]{};
    MisakiPM424Report report{};
    check(misaki_pm424_diagnostic(rows, 2, &report) == 0, "bridge returns state trace");
    check(report.abi_version == 1 && report.packets == 11 &&
          report.indirect_buffers == 2 && report.register_writes == 8,
          "bridge trace metadata");
    check(report.draws == 1 && report.ready_draws == 1 && report.rejected_draws == 0,
          "bridge state completeness");
    check(rows[0].state_status == 0 && rows[0].vertex_count == 3,
          "bridge draw state status and vertex count");
    check(rows[0].color_address == (uint64_t(0x200000) << 8) &&
          rows[0].pixel_shader_address == (uint64_t(0x1000) << 8),
          "bridge derived addresses");
    check(rows[0].scissor_x == 10 && rows[0].scissor_y == 10 &&
          rows[0].scissor_width == 190 && rows[0].scissor_height == 110,
          "bridge scissor metadata");
    check(rows[0].pitch_tile_max == 127 && rows[0].color_format_field == 27,
          "bridge raw metadata");
    check(rows[0].packet_address == d.packetAddress && rows[0].depth == 1,
          "bridge guest address and depth");
    check(report.checksum == normal->checksum, "bridge/engine checksum match");
    rows[0].state_status = 999;
    check(misaki_pm424_diagnostic(rows, 0, &report) == -3,
          "bridge insufficient capacity fails before writes");
    check(rows[0].state_status == 999 && report.draws == 0,
          "failed call leaves output unchanged and resets report");
    check(misaki_pm424_diagnostic(nullptr, 2, &report) == -1,
          "null array rejected");
    check(misaki_pm424_diagnostic(rows, 2, nullptr) == -1,
          "null report rejected");
    check(misaki_pm424_diagnostic(nullptr, 0, &report) == -3,
          "zero capacity rejected");

    // Iterated scissor perturbations exercise metadata rejection and arithmetic.
    for (std::uint32_t x = 0; x < 60; ++x) {
        auto varied = fixture;
        varied.grandchildWords[2] = 0x000A0000u | (10u + x);
        const auto out = run(varied);
        check(out && out->draws.size() == 1 && out->readyDraws == 1 &&
              out->draws[0].scissorWidth == 190 - x,
              "scissor width tracks distinct test coordinates");
    }
    std::cout << "PASS: " << assertions << " PM4 draw-state assertions\n";
}
