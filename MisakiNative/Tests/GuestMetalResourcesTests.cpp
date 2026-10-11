#include "GuestMetalResources.hpp"
#include "MisakiMetalResourcesBridge.h"

#include <array>
#include <cstdint>
#include <cstdlib>
#include <iostream>
#include <limits>
#include <string>
#include <vector>

namespace {
int checks = 0;
void check(bool condition, const char *label) {
    ++checks;
    if (!condition) { std::cerr << "FAIL: " << label << '\n'; std::exit(1); }
}
void put32(std::uint8_t *data, std::uint32_t n) {
    for (unsigned i=0; i<4; ++i) data[i] = static_cast<std::uint8_t>(n >> (8*i));
}
}
int main() {
    using namespace misaki;
    std::string err;
    const auto program = makeResource28Program();
    const auto gcn = runGCNShaderDiagnostic();
    const auto ir = runMetalIRDiagnostic();
    check(bool(gcn) && bool(ir), "M25 and M27 pipelines available");
    const auto desc = gcn->buffer;
    check(desc.supportedLinearMetadata && desc.baseAddress == 0x400000u &&
          desc.records == 64 && desc.strideBytes == 16, "genuine-format resource descriptor fields");
    check(program.instructions.size() == 3, "three typed resource operations");
    check(program.instructions[0].opcode == Resource28Opcode::loadU32 &&
          program.instructions[1].opcode == Resource28Opcode::addRegister &&
          program.instructions[2].opcode == Resource28Opcode::storeU32,
          "typed IO order load-add-store");
    auto metal = translateResource28ToMetal(program, desc, &err);
    check(metal && err.empty(), "MSL source generation");
    check(metal->find("kernel void misaki_resource28") != std::string::npos, "resource entry");
    check(metal->find("[[buffer(0)]]") != std::string::npos &&
          metal->find("[[buffer(1)]]") != std::string::npos &&
          metal->find("[[buffer(2)]]") != std::string::npos &&
          metal->find("[[buffer(3)]]") != std::string::npos, "four Metal buffers");
    check(metal->find("readIndex * 4u") != std::string::npos, "respects 16-byte descriptor stride");
    check(metal->find("registers[1] < 1u") != std::string::npos,
          "negative index offset guarded before GPU read");
    check(metal->find("readIndex >= 64u") != std::string::npos &&
          metal->find("writeIndex >= 64u") != std::string::npos,
          "bounds checked for both GPU buffers");
    check(metal->find("outputWords[writeIndex] = result") != std::string::npos &&
          metal->find("status[0] = 0u") != std::string::npos,
          "GPU writes output and success flag");
    check(metal->size() < 4096 && translateResource28ToMetal(program, desc) == metal,
          "bounded deterministic generated source");

    GuestMemory guest;
    std::array<std::uint8_t, 1024> bytes{};
    for (std::size_t i=0; i<64; ++i) put32(bytes.data()+16*i, static_cast<std::uint32_t>(i*3+10));
    check(guest.map(desc.baseAddress, bytes.size(), permission::read | permission::write),
          "map guest descriptor backing memory");
    check(guest.writeBytes(desc.baseAddress, bytes.data(), bytes.size()), "install guest bytes");
    check(guest.protect(desc.baseAddress, bytes.size(), permission::read), "protect guest input");
    check(!guest.writeBytes(desc.baseAddress, bytes.data(), 1), "guest input is read only");
    std::array<std::uint32_t, 64> initial{};
    for (std::size_t i=0; i<initial.size(); ++i) initial[i] = 0xA0000000u + i;
    auto evaluated = evaluateResource28(program, guest, desc, ir->expected, initial, &err);
    check(evaluated && err.empty(), "CPU resource evaluator");
    check(evaluated->readIndex == 7 && evaluated->writeIndex == 6,
          "indices derive from M27 scalar registers");
    check(evaluated->loadedWord == 31 && evaluated->storedWord == 45,
          "load + add with actual source records");
    check(evaluated->output[6] == 45 && evaluated->output[7] == initial[7],
          "only selected output record changed");
    for (std::size_t i=0; i<initial.size(); ++i)
        check(evaluated->output[i] == (i==6 ? 45u : initial[i]),
              "all unwritten output records remain intact");

    auto mutatedRegs = ir->expected;
    mutatedRegs[0] = 0;
    check(evaluateResource28(program, guest, desc, mutatedRegs, initial)->loadedWord == 10,
          "first record read permitted");
    mutatedRegs[0] = 63;
    check(evaluateResource28(program, guest, desc, mutatedRegs, initial)->loadedWord == 199,
          "last record read permitted");
    mutatedRegs[0] = 64;
    check(!evaluateResource28(program, guest, desc, mutatedRegs, initial, &err) &&
          !err.empty(), "index beyond record limit rejected");
    mutatedRegs = ir->expected; mutatedRegs[1] = 0;
    check(!evaluateResource28(program, guest, desc, mutatedRegs, initial),
          "negative write-index underflow rejected");
    mutatedRegs[1] = 64;
    check(evaluateResource28(program, guest, desc, mutatedRegs, initial)->writeIndex == 63,
          "upper boundary write index accepted");
    mutatedRegs[1] = 65;
    check(!evaluateResource28(program, guest, desc, mutatedRegs, initial),
          "write index beyond upper boundary rejected");
    mutatedRegs = ir->expected; mutatedRegs[2] = 0xFFFFFFFFu;
    check(evaluateResource28(program, guest, desc, mutatedRegs, initial)->storedWord == 30,
          "32-bit integer add uses modulo arithmetic");

    auto malformed = program; malformed.instructions.pop_back();
    check(!evaluateResource28(malformed, guest, desc, ir->expected, initial) &&
          !translateResource28ToMetal(malformed, desc), "missing store rejected");
    malformed = program; malformed.instructions[0].opcode = Resource28Opcode::storeU32;
    check(!evaluateResource28(malformed, guest, desc, ir->expected, initial) &&
          !translateResource28ToMetal(malformed, desc), "store-before-load rejected");
    malformed = program; malformed.instructions[1].indexOffset = 1;
    check(!evaluateResource28(malformed, guest, desc, ir->expected, initial) &&
          !translateResource28ToMetal(malformed, desc), "unexpected ALU index offset rejected");
    malformed = program; malformed.instructions[0].referenceRegister = 6;
    check(!evaluateResource28(malformed, guest, desc, ir->expected, initial) &&
          !translateResource28ToMetal(malformed, desc), "invalid source register rejected");
    malformed = program; malformed.instructions[2].indexOffset = 33;
    check(!evaluateResource28(malformed, guest, desc, ir->expected, initial) &&
          !translateResource28ToMetal(malformed, desc), "excessive index bias rejected");
    malformed = program; malformed.instructions[1].opcode = static_cast<Resource28Opcode>(255);
    check(!evaluateResource28(malformed, guest, desc, ir->expected, initial) &&
          !translateResource28ToMetal(malformed, desc), "unknown IR opcode rejected");
    malformed = program; malformed.instructions.push_back(program.instructions.back());
    check(!evaluateResource28(malformed, guest, desc, ir->expected, initial) &&
          !translateResource28ToMetal(malformed, desc), "duplicate store rejected");

    auto badDesc = desc; badDesc.supportedLinearMetadata = false;
    check(!evaluateResource28(program, guest, badDesc, ir->expected, initial) &&
          !translateResource28ToMetal(program, badDesc), "unsupported AMD resource format rejected");
    badDesc = desc; badDesc.strideBytes = 0;
    check(!evaluateResource28(program, guest, badDesc, ir->expected, initial),
          "missing buffer stride rejected");
    badDesc = desc; badDesc.strideBytes = 12;
    check(!translateResource28ToMetal(program, badDesc), "unsupported stride refused");
    badDesc = desc; badDesc.records = 128;
    check(!evaluateResource28(program, guest, badDesc, ir->expected, initial),
          "unsupported record count refused");
    badDesc = desc; badDesc.baseAddress = UINT64_MAX - 1;
    check(!evaluateResource28(program, guest, badDesc, ir->expected, initial),
          "overflowing resource bounds refused");
    GuestMemory inaccessible;
    check(!evaluateResource28(program, inaccessible, desc, ir->expected, initial, &err),
          "unmapped guest resource refused");
    check(!err.empty(), "read permission error surfaced");
    check(guest.protect(desc.baseAddress, bytes.size(), permission::write),
          "remove guest read permission");
    check(!evaluateResource28(program, guest, desc, ir->expected, initial),
          "write-only guest resource not readable");

    const auto plan = runMetalResource28Diagnostic();
    check(bool(plan), "integrated M25 descriptor + M27 shader + M28 resources");
    check(plan->irChecksum == ir->irChecksum && plan->registers == ir->expected,
          "resource stage coupled to original translated IR");
    check(plan->guestInputReadOnly && plan->guestOutputWritable,
          "read-only guest input and writable guest output exercised");
    check(plan->resourceMSL == *metal && plan->sourceHash != 0,
          "integrated generated MSL identical to independent translator");
    check(plan->descriptorBase == 0x400000u && plan->descriptorStride == 16 &&
          plan->descriptorRecords == 64, "descriptor exported");
    check(plan->readIndex == 7 && plan->writeIndex == 6 &&
          plan->loadedWord == 31 && plan->storedWord == 45, "expected resource result");
    check(plan->expectedHash != plan->initialHash &&
          plan->outputExpected[6] == 45u &&
          plan->outputInitial[6] != plan->outputExpected[6],
          "output fingerprint depends on a true memory write");
    const auto again = runMetalResource28Diagnostic();
    check(again && plan->resourceMSL == again->resourceMSL &&
          plan->expectedHash == again->expectedHash &&
          plan->sourceHash == again->sourceHash,
          "integrated reference deterministic");

    char source[4096]{};
    std::uint8_t input[1024]{};
    std::uint32_t output[64]{};
    std::uint32_t expected[64]{};
    MisakiMetalResource28Report report{};
    check(misaki_metal_resource28_plan(source, sizeof(source), input, sizeof(input),
                                       output, 64, expected, 64, &report) == 0,
          "complete C bridge resource export");
    check(report.abi_version == 1 && report.input_bytes == 1024 &&
          report.output_words == 64 && report.resource_ops == 3 &&
          report.source_bytes == plan->resourceMSL.size()+1,
          "C ABI dimensions and counts");
    check(report.ir_checksum == plan->irChecksum &&
          report.metal_source_hash == plan->sourceHash &&
          report.output_expected_hash == plan->expectedHash &&
          report.output_initial_hash == plan->initialHash, "C ABI hashes");
    check(report.descriptor_base == plan->descriptorBase &&
          report.descriptor_records == 64 && report.descriptor_stride == 16 &&
          report.guest_input_read_only == 1 && report.guest_output_writable == 1,
          "C ABI descriptor/protection");
    check(report.read_index == 7 && report.write_index == 6 &&
          report.loaded_word == 31 && report.stored_word == 45,
          "C ABI guest IO values");
    check(std::string(source) == *metal &&
          std::equal(std::begin(input), std::end(input), plan->guestBytes.begin()),
          "C ABI source and input equal native plan");
    for (std::size_t i=0; i<64; ++i)
        check(output[i] == plan->outputInitial[i] &&
              expected[i] == plan->outputExpected[i], "C ABI full output arrays");
    source[0] = 'X'; input[0] = 0xCC; output[0] = 0xABCDEF01u; expected[0] = 0x43218765u;
    check(misaki_metal_resource28_plan(source, 1, input, sizeof(input),
                                       output, 64, expected, 64, &report) == -3,
          "undersized MSL output rejected");
    check(source[0] == 'X' && input[0] == 0xCC && output[0] == 0xABCDEF01u &&
          expected[0] == 0x43218765u && report.abi_version == 0,
          "too-small shader output rejected without partial writes");
    check(misaki_metal_resource28_plan(source, sizeof(source), input, 1023,
                                       output, 64, expected, 64, &report) == -3,
          "undersized input rejected");
    check(source[0] == 'X' && input[0] == 0xCC,
          "too-small guest input has no partial writes");
    check(misaki_metal_resource28_plan(source, sizeof(source), input, sizeof(input),
                                       output, 63, expected, 64, &report) == -3,
          "undersized initial output rejected");
    check(misaki_metal_resource28_plan(source, sizeof(source), input, sizeof(input),
                                       output, 64, expected, 63, &report) == -3,
          "undersized expected output rejected");
    check(misaki_metal_resource28_plan(source, sizeof(source), input, sizeof(input),
                                       output, 64, expected, 64, nullptr) == -1,
          "null report rejected");
    check(misaki_metal_resource28_plan(nullptr, 4096, input, sizeof(input),
                                       output, 64, expected, 64, &report) == -1,
          "null source pointer with nonzero capacity rejected");
    check(misaki_metal_resource28_plan(source, sizeof(source), nullptr, sizeof(input),
                                       output, 64, expected, 64, &report) == -1,
          "null input pointer with capacity rejected");
    check(misaki_metal_resource28_plan(nullptr, 0, input, sizeof(input),
                                       output, 64, expected, 64, &report) == -3,
          "zero source capacity safely rejected");

    // Stable stress coverage for invalid references and out-of-range access.
    std::uint32_t seed = 0x18812345u;
    for (unsigned i=0; i<500; ++i) {
        seed ^= seed << 13; seed ^= seed >> 17; seed ^= seed << 5;
        auto regs = ir->expected;
        regs[0] = seed;
        regs[1] = seed >> 13;
        auto eval = evaluateResource28(program, guest, desc, regs, initial);
        check(!eval, "inaccessible guest and arbitrary registers fail closed");
    }
    std::cout << "PASS: " << checks << " GPU resource and GCN IR assertions\n";
}
