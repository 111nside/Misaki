#include "GuestFusedGCN30.hpp"
#include "MisakiFusedGCN30Bridge.h"

#include <array>
#include <cstdlib>
#include <iostream>
#include <limits>
#include <string>
#include <vector>

namespace {
int checks = 0;
void check(bool value, const char *what) {
    ++checks;
    if (!value) { std::cerr << "FAIL: " << what << '\n'; std::exit(1); }
}
void put32(std::uint8_t *dst, std::uint32_t word) {
    for (unsigned i = 0; i < 4; ++i) dst[i] = static_cast<std::uint8_t>(word >> (8 * i));
}
std::vector<std::uint8_t> bytesOf(const std::vector<std::uint32_t> &words) {
    std::vector<std::uint8_t> result(words.size() * 4);
    for (size_t i = 0; i < words.size(); ++i) put32(result.data() + 4 * i, words[i]);
    return result;
}
struct MemoryFixture {
    misaki::GuestMemory guest;
    std::array<std::uint32_t, 64> initial{};
    std::vector<std::uint32_t> code = misaki::makeFusedGCN30Fixture();
    bool setup() {
        const auto bytes = bytesOf(code);
        if (!guest.map(0x100000, bytes.size(), misaki::permission::read | misaki::permission::write) ||
            !guest.writeBytes(0x100000, bytes.data(), bytes.size()) ||
            !guest.protect(0x100000, bytes.size(), misaki::permission::read)) return false;
        const auto a = misaki::makeGCNBufferFixture();
        auto b = a;
        b[0] = 0x500000;
        b[1] = 4 << 16;
        if (!guest.map(0x110000, 32, misaki::permission::read | misaki::permission::write) ||
            !guest.writeBytes(0x110000, bytesOf({a.begin(), a.end()}).data(), 16) ||
            !guest.writeBytes(0x110010, bytesOf({b.begin(), b.end()}).data(), 16) ||
            !guest.protect(0x110000, 32, misaki::permission::read)) return false;
        std::vector<std::uint8_t> input(1024);
        std::vector<std::uint8_t> output(256);
        for (unsigned i = 0; i < 64; ++i) {
            put32(input.data() + i * 16, i * 3 + 10);
            initial[i] = 0xB0000000u + i;
            put32(output.data() + i * 4, initial[i]);
        }
        return guest.map(0x400000, input.size(), misaki::permission::read | misaki::permission::write) &&
            guest.writeBytes(0x400000, input.data(), input.size()) &&
            guest.protect(0x400000, input.size(), misaki::permission::read) &&
            guest.map(0x500000, output.size(), misaki::permission::read | misaki::permission::write) &&
            guest.writeBytes(0x500000, output.data(), output.size());
    }
};
}
int main() {
    using namespace misaki;
    const auto words = makeFusedGCN30Fixture();
    check(words.size() == 13, "13 shader DWORDs");
    auto decoded = decodeFusedGCN30(words.data(), words.size(), 0x100000);
    check(bool(decoded), "decode fully interleaved GCN shader");
    check(decoded->steps.size() == 11 && decoded->codeWords == 13,
          "11 ordered instructions, including two 64-bit MUBUF operations");
    check(decoded->aluOperations == 8 && decoded->bufferLoads == 1 &&
          decoded->bufferStores == 1, "8 ALU operations and two memory operations");
    check(decoded->steps[6].kind == FusedGCN30Kind::memory &&
          decoded->steps[6].wordOffset == 6, "MUBUF load between ALU instructions");
    check(decoded->steps[7].kind == FusedGCN30Kind::alu &&
          decoded->steps[7].wordOffset == 8, "vector move after loaded word");
    check(decoded->steps[9].kind == FusedGCN30Kind::memory &&
          decoded->steps[9].memory.vdata == 9, "GCN store observes v9");
    check(decoded->steps.back().kind == FusedGCN30Kind::end, "program terminates");
    check(decoded->checksum != 0 &&
          decodeFusedGCN30(words.data(),words.size(),0x100000)->checksum == decoded->checksum,
          "deterministic decoded checksum");
    check(!decodeFusedGCN30(nullptr, words.size(), 0x100000), "null words rejected");
    check(!decodeFusedGCN30(words.data(), 0, 0x100000), "empty words rejected");
    check(!decodeFusedGCN30(words.data(), 65, 0x100000), "oversized shader rejected");
    check(!decodeFusedGCN30(words.data(), words.size(), 0x100004), "misalignment rejected");
    check(!decodeFusedGCN30(words.data(), words.size(), UINT64_MAX-7), "overflow rejected");
    for (std::size_t end = 1; end < words.size(); ++end) {
        check(!decodeFusedGCN30(words.data(), end, 0x100000), "truncated source rejected");
    }
    for (std::size_t location = 0; location < words.size(); ++location) {
        auto changed = words;
        changed[location] = 0xFFFFFFFFu;
        check(!decodeFusedGCN30(changed.data(), changed.size(), 0x100000),
              "invalid instruction encoding rejected");
    }
    auto early = words;
    early[5] = 0xBF810000u;
    check(!decodeFusedGCN30(early.data(), early.size(), 0x100000), "early termination rejected");
    auto badAddressing = words;
    badAddressing[6] |= 1u << 12;
    check(!decodeFusedGCN30(badAddressing.data(), badAddressing.size(), 0x100000),
          "unsupported OFFEN flag rejected");
    badAddressing = words;
    badAddressing[7] ^= 1u << 22;
    check(!decodeFusedGCN30(badAddressing.data(), badAddressing.size(), 0x100000),
          "unsupported SLC flag rejected");
    badAddressing = words;
    badAddressing[10] ^= 0x00040000u;
    check(!decodeFusedGCN30(badAddressing.data(), badAddressing.size(), 0x100000),
          "modified destination register rejected");
    MemoryFixture fixture;
    check(fixture.setup(), "create restricted guest-memory fixture");
    auto guestDecoded = decodeGuestFusedGCN30(fixture.guest, 0x100000, words.size());
    check(guestDecoded && guestDecoded->checksum == decoded->checksum,
          "guest-read instruction sequence matches direct decode");
    check(!decodeGuestFusedGCN30(fixture.guest, 0x100000, words.size()+1),
          "guest instruction buffer rejects unmapped tail");
    check(!decodeGuestFusedGCN30(fixture.guest, 0x900000, words.size()),
          "unmapped guest shader rejected");
    check(!fixture.guest.writeBytes(0x100000, reinterpret_cast<const uint8_t*>(words.data()), 1),
          "guest instruction memory cannot be written");
    auto input = decodeGuestGCNBuffer(fixture.guest, 0x110000);
    auto output = decodeGuestGCNBuffer(fixture.guest, 0x110010);
    check(input && output, "both resource descriptors decode");
    const auto translated = translateFusedGCN30ToMetal(*decoded, *input, *output);
    check(translated && translated->find("kernel void misaki_fused30") != std::string::npos,
          "real generated one-pass Metal compute shader");
    check(translated->find("v[8] = inputWords[") != std::string::npos &&
          translated->find("outputWords[") != std::string::npos,
          "load/store follow interleaved ALU commands");
    check(translated->find("v[9] = v[8]") != std::string::npos,
          "GPU data moved between VGPRs after load");
    check(translated->size() < 8192 &&
          *translated == *translateFusedGCN30ToMetal(*decoded,*input,*output),
          "bounded deterministic shader source");
    auto result = executeFusedGCN30(*decoded, fixture.guest, 0x110000, 0x110010,
                                     fixture.initial);
    check(bool(result), "native mixed shader executes");
    check(result->executed == 11 && result->index == 7 && result->loaded == 31 &&
          result->stored == 31, "all instructions execute and loaded value stored");
    check(result->registers == std::array<uint32_t,8>{{7,7,14,25,18,7,31,31}},
          "scalar/vector registers hold expected values");
    for (unsigned i = 0; i < 64; ++i)
        check(result->output[i] == (i == 7 ? 31u : fixture.initial[i]),
              "exactly one output record changed");
    MemoryFixture rejected;
    check(rejected.setup(), "recreate guest fixture for permissions test");
    check(rejected.guest.protect(0x500000, 256, permission::read),
          "make output buffer read-only");
    check(!executeFusedGCN30(*decoded,rejected.guest,0x110000,0x110010,rejected.initial),
          "read-only destination causes failure");
    check(rejected.guest.read64(0x500000 + 7 * 4) ==
          fixture.initial[7] + (uint64_t(fixture.initial[8]) << 32),
          "failed execution did not partially write output");
    MemoryFixture unreadable;
    check(unreadable.setup(), "recreate guest fixture for input permission test");
    check(unreadable.guest.protect(0x400000, 1024, permission::write),
          "make input unreadable");
    check(!executeFusedGCN30(*decoded,unreadable.guest,0x110000,0x110010,unreadable.initial),
          "unreadable input rejected");
    MemoryFixture invalidIndex;
    check(invalidIndex.setup(), "recreate fixture for out-of-range index");
    auto outOfRange = makeFusedGCN30Fixture();
    outOfRange[0] = 0xB0000000u | (1u << 16) | 64u;
    const auto p = decodeFusedGCN30(outOfRange.data(), outOfRange.size(), 0x100000);
    check(p && !executeFusedGCN30(*p, invalidIndex.guest, 0x110000,0x110010,
                                   invalidIndex.initial), "index 64 fails before load/store");
    auto tampered = *decoded;
    tampered.steps[6].memory.vdata = 250;
    check(!executeFusedGCN30(tampered, invalidIndex.guest,0x110000,0x110010,
                              invalidIndex.initial), "tampered decoded MUBUF rejected");
    tampered = *decoded;
    tampered.steps[2].alu.destination = 255;
    check(!translateFusedGCN30ToMetal(tampered,*input,*output),
          "tampered IR destination rejected");
    auto plan = runFusedGCN30Diagnostic();
    check(plan && plan->codeReadOnly && plan->descriptorsReadOnly &&
          plan->inputReadOnly && plan->outputWritable, "integrated plan verifies guest permissions");
    check(plan->expected.registers == result->registers && plan->expected.output == result->output,
          "integrated plan and standalone evaluator agree");
    check(plan->inputBase == 0x400000 && plan->outputBase == 0x500000,
          "guest memory uses descriptor-defined buffers");
    char source[8192]{};
    uint8_t inputBytes[1024]{};
    uint32_t initial[64]{}, expected[64]{}, registers[8]{};
    MisakiFusedGCN30Report report{};
    check(misaki_gcn30_plan(source, 8192, inputBytes, 1024, initial, 64,
                            expected, 64, registers, 8, &report) == 0,
          "C bridge returns fused shader plan");
    check(report.abi_version == 1 && report.instructions == 11 &&
          report.code_words == 13 && report.alu_operations == 8 &&
          report.buffer_loads == 1 && report.buffer_stores == 1 && report.executed == 11,
          "C bridge reports fused shader steps");
    check(report.register_words == 8 && report.output_words == 64 &&
          report.input_bytes == 1024 && report.source_bytes > 1 &&
          report.source_bytes < 8192, "C bridge reports bounded buffers");
    check(report.record_index == 7 && report.loaded == 31 && report.stored == 31 &&
          report.code_read_only && report.descriptors_read_only &&
          report.input_read_only && report.output_writable,
          "C bridge reports diagnostics and permissions");
    check(source[report.source_bytes - 1] == 0 &&
          std::string(source) == plan->metalSource, "full native Metal source copied");
    for (unsigned i = 0; i < 1024; ++i)
        check(inputBytes[i] == plan->inputBytes[i], "input byte serialized correctly");
    for (unsigned i = 0; i < 64; ++i) {
        check(initial[i] == plan->initial[i], "initial framebuffer records match");
        check(expected[i] == plan->expected.output[i], "expected framebuffer records match");
    }
    for (unsigned i = 0; i < 8; ++i)
        check(registers[i] == plan->expected.registers[i], "expected register value matches");
    source[0] = '@';
    check(misaki_gcn30_plan(source, 1, inputBytes, 1024, initial,64,
                            expected,64,registers,8,&report) == -3 &&
          source[0] == '@' && report.abi_version == 0,
          "bridge refuses small source capacity with no partial writes");
    inputBytes[0] = 0xAA;
    check(misaki_gcn30_plan(source,8192,inputBytes,1,initial,64,
                            expected,64,registers,8,&report) == -3 && inputBytes[0] == 0xAA,
          "bridge refuses small input capacity without partial writes");
    check(misaki_gcn30_plan(source,8192,inputBytes,1024,initial,64,
                            expected,64,registers,7,&report) == -3,
          "bridge refuses insufficient register output capacity");
    check(misaki_gcn30_plan(nullptr,8192,inputBytes,1024,initial,64,
                            expected,64,registers,8,&report) == -1,
          "bridge rejects null output");
    check(misaki_gcn30_plan(source,8192,inputBytes,1024,initial,64,
                            expected,64,registers,8,nullptr) == -1,
          "bridge rejects null report");
    uint32_t seed=0xCAFE1234u;
    for (unsigned j=0;j<500;++j) {
        auto candidate = words;
        for (unsigned k=0;k<3;++k) {
            seed ^= seed<<13; seed ^= seed>>17; seed ^= seed<<5;
            const unsigned at = seed % static_cast<unsigned>(candidate.size());
            candidate[at] ^= seed;
        }
        auto randomDecode = decodeFusedGCN30(candidate.data(),candidate.size(),0x100000);
        check(!randomDecode || randomDecode->steps.size() <= candidate.size(),
              "mutated shader decoder stays within bounded instruction count");
    }
    std::cout << "PASS: " << checks << " fused GCN shader assertions\n";
}
