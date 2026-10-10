#include "GuestGCNISA.hpp"
#include "MisakiGCN25Bridge.h"

#include <array>
#include <cstdint>
#include <cstdlib>
#include <iostream>
#include <limits>
#include <string>
#include <vector>

namespace {
int checks = 0;
void check(bool good, const char *message) {
    ++checks;
    if (!good) { std::cerr << "FAIL: " << message << "\n"; std::exit(1); }
}
void reject(std::vector<uint32_t> input, size_t at, uint32_t change, const char *reason) {
    input.at(at) = change;
    std::string error;
    check(!misaki::decodeGCNShaderWords(input.data(), input.size(), 0x100000, &error), reason);
    check(!error.empty(), "bad shader gives error reason");
}
std::vector<uint8_t> bytes(const std::vector<uint32_t> &words) {
    std::vector<uint8_t> out;
    for (const auto word : words) for (unsigned j = 0; j < 32; j += 8)
        out.push_back(static_cast<uint8_t>(word >> j));
    return out;
}
} // namespace
int main() {
    using namespace misaki;
    const auto shader = makeGCNShaderFixture();
    std::string err;
    auto decoded = decodeGCNShaderWords(shader.data(), shader.size(), 0x100000, &err);
    check(bool(decoded), "decode original-format GCN shader instructions");
    check(err.empty(), "successful decode clears error");
    check(decoded->instructions.size() == 8, "eight shader instructions");
    check(decoded->scalarInstructions == 5, "five scalar instructions");
    check(decoded->vectorInstructions == 3, "three vector instructions");
    check(decoded->literalWords == 1 && decoded->totalWords == 9, "literal uses one extra DWORD");
    check(decoded->ended && decoded->checksum != 0, "S_ENDPGM terminated and checksummed");
    check(decoded->instructions[0].encoding == GCNEncoding::sopk &&
          decoded->instructions[0].opcode == 0 && decoded->instructions[0].dst == 1 &&
          decoded->instructions[0].src0 == 7, "S_MOVK_I32 immediate fields");
    check(decoded->instructions[1].encoding == GCNEncoding::sop1 &&
          decoded->instructions[1].dst == 2 && decoded->instructions[1].src0 == 1,
          "S_MOV_B32 fields");
    check(decoded->instructions[2].encoding == GCNEncoding::sop2 &&
          decoded->instructions[2].dst == 3 && decoded->instructions[2].src0 == 1 &&
          decoded->instructions[2].src1 == 2, "S_ADD_U32 fields");
    check(decoded->instructions[3].encoding == GCNEncoding::vop1 &&
          decoded->instructions[3].src0 == 257, "V_MOV_B32 VGPR source");
    check(decoded->instructions[4].encoding == GCNEncoding::vop2 &&
          decoded->instructions[4].dst == 1 && decoded->instructions[4].src0 == 256 &&
          decoded->instructions[4].src1 == 0, "V_ADD_F32 register fields");
    check(decoded->instructions[5].encoding == GCNEncoding::vop2 &&
          decoded->instructions[5].hasLiteral && decoded->instructions[5].wordCount == 2 &&
          decoded->instructions[5].literal == 0x3F800000u,
          "V_MUL_F32 literal consumes trailing DWORD");
    check(decoded->instructions[6].mnemonic == "s_nop" &&
          decoded->instructions[7].mnemonic == "s_endpgm", "scalar control opcodes");
    check(decoded->instructions[7].guestAddress == 0x100020 &&
          decoded->instructions[7].wordOffset == 8, "PC counts extra literal word");
    check(decodeGCNShaderWords(shader.data(), shader.size(), 0x100000)->checksum ==
          decoded->checksum, "deterministic trace hash");
    check(!decodeGCNShaderWords(nullptr, 4, 0x100000), "null shader input");
    check(!decodeGCNShaderWords(shader.data(), 0, 0x100000), "zero length rejected");
    check(!decodeGCNShaderWords(shader.data(), 1025, 0x100000), "huge length rejected");
    check(!decodeGCNShaderWords(shader.data(), shader.size(), 0x100001), "unaligned PC rejected");
    check(!decodeGCNShaderWords(shader.data(), shader.size(), UINT64_MAX - 7),
          "overflowing guest shader address rejected");
    for (size_t prefix = 1; prefix < shader.size(); ++prefix)
        check(!decodeGCNShaderWords(shader.data(), prefix, 0x100000), "incomplete shader rejected");
    reject(shader, 0, 0xB0000000 | (127u << 16) | 1u, "SOPK inaccessible SGPR");
    reject(shader, 1, 0xBE800000 | (2u << 16) | (127u), "SOP1 reserved source");
    reject(shader, 2, 0x80000000 | (102u << 16) | (2u << 8) | 1u, "SOP2 unsupported dest");
    reject(shader, 2, 0x80000000 | (3u << 16) | (255u << 8) | 255u,
           "SOP2 two literals not supported");
    reject(shader, 3, 0x7E000000 | (19u << 9) | 256u, "unsupported VOP1 opcode");
    reject(shader, 4, (63u << 25) | 256u, "unsupported VOP2 opcode");
    reject(shader, 7, 0xBF820000, "unsupported control flow branch");
    reject(shader, 8, 0xBF810001, "S_ENDPGM reserved immediate bits");
    reject(shader, 8, 0xBF800000, "shader must finish with S_ENDPGM");
    reject(shader, 0, 0xD0000000, "reject 64-bit instruction encoding");
    reject(shader, 0, 0xBF000000, "reject SOPC until implemented");
    reject(shader, 0, 0x7C000000, "reject VOPC until implemented");
    auto trailing = shader;
    trailing.push_back(0xBF800000);
    check(!decodeGCNShaderWords(trailing.data(), trailing.size(), 0x100000),
          "do not ignore hidden trailing shader words");
    const std::vector<uint32_t> directLiteral = {0xBE8000FF, 0xDEADBEEF, 0xBF810000};
    auto direct = decodeGCNShaderWords(directLiteral.data(), directLiteral.size(), 0x200000);
    check(direct && direct->literalWords == 1 && direct->instructions.size() == 2 &&
          direct->instructions[0].literal == 0xDEADBEEF, "SOP1 literal constant decoded");
    const std::vector<uint32_t> invalidLiteral = {0xBE8000FF};
    check(!decodeGCNShaderWords(invalidLiteral.data(), invalidLiteral.size(), 0x200000),
          "reject truncated literal");
    const std::vector<uint32_t> doubleEnd = {0xBF810000, 0xBF810000};
    check(!decodeGCNShaderWords(doubleEnd.data(), doubleEnd.size(), 0x200000),
          "reject multiple S_ENDPGM opcodes");
    const auto bufferWords = makeGCNBufferFixture();
    auto desc = decodeGCNBufferWords(bufferWords);
    check(desc.baseAddress == 0x400000 && desc.strideBytes == 16 && desc.records == 64,
          "buffer address, stride and records");
    check(desc.channelSelectors == std::array<uint32_t,4>{{4,5,6,7}},
          "resource channel selectors");
    check(desc.dataFormat == 14 && desc.numericFormat == 0 &&
          desc.resourceType == 0 && desc.supportedLinearMetadata,
          "linear resource format recognized without executing");
    for (unsigned i = 0; i < 4; ++i) {
        auto mutated = bufferWords;
        mutated[3] = (mutated[3] & ~(7u << (i * 3))) | (2u << (i * 3));
        check(!decodeGCNBufferWords(mutated).supportedLinearMetadata,
              "reserved swizzle channel selector rejected");
    }
    for (uint32_t flag : {1u << 31, 1u << 23, 1u << 24}) {
        auto mutated = bufferWords;
        mutated[(flag == (1u << 31)) ? 1 : 3] |= flag;
        check(!decodeGCNBufferWords(mutated).supportedLinearMetadata,
              "unsupported resource memory mode rejected");
    }
    for (unsigned part = 0; part < 4; ++part) {
        auto mutated = bufferWords;
        mutated[part] = 0;
        if (part == 0 || part == 1 || part == 2 || part == 3)
            check(!decodeGCNBufferWords(mutated).supportedLinearMetadata,
                  "required resource metadata absent");
    }
    auto wrongType = bufferWords;
    wrongType[3] |= (1u << 30);
    check(!decodeGCNBufferWords(wrongType).supportedLinearMetadata,
          "reject non-buffer descriptor type");
    auto shaderBytes = bytes(shader);
    GuestMemory memory;
    check(memory.map(0x200000, shaderBytes.size(), permission::read | permission::write),
          "map guest shader bytes");
    check(memory.writeBytes(0x200000, shaderBytes.data(), shaderBytes.size()),
          "copy guest shader bytes");
    check(memory.protect(0x200000, shaderBytes.size(), permission::read),
          "protect shader bytes as read-only");
    auto guest = decodeGuestGCNShader(memory, 0x200000, shader.size());
    check(guest && guest->checksum == decoded->checksum,
          "decode shader from protected guest memory");
    check(!decodeGuestGCNShader(memory, 0x300000, shader.size()),
          "reject unmapped guest shader");
    check(!decodeGuestGCNShader(memory, UINT64_MAX, 1),
          "reject overflowing guest shader pointer");
    check(!decodeGuestGCNShader(memory, 0x200000, shader.size() + 1),
          "reject shader range crossing mapping");
    check(memory.protect(0x200000, shaderBytes.size(), permission::write),
          "revoke shader read permissions");
    check(!decodeGuestGCNShader(memory, 0x200000, shader.size()),
          "enforce shader read permissions");
    auto bufferBytes = bytes({bufferWords.begin(), bufferWords.end()});
    check(memory.map(0x300000, bufferBytes.size(), permission::read | permission::write),
          "map guest resource descriptor");
    check(memory.writeBytes(0x300000, bufferBytes.data(), bufferBytes.size()),
          "copy guest resource descriptor");
    check(memory.protect(0x300000, bufferBytes.size(), permission::read),
          "protect descriptor read-only");
    auto fromGuest = decodeGuestGCNBuffer(memory, 0x300000);
    check(fromGuest && fromGuest->baseAddress == 0x400000 &&
          fromGuest->supportedLinearMetadata, "read protected descriptor");
    check(!decodeGuestGCNBuffer(memory, 0x300001), "reject unaligned resource descriptor");
    check(!decodeGuestGCNBuffer(memory, UINT64_MAX), "reject overflowing resource address");
    check(memory.protect(0x300000, bufferBytes.size(), permission::write),
          "remove descriptor read permission");
    check(!decodeGuestGCNBuffer(memory, 0x300000), "descriptor respects read permissions");
    auto integrated = runGCNShaderDiagnostic();
    check(bool(integrated), "PM4 pixel shader links to GCN shader decode");
    check(integrated->shader.instructions.size() == 8 &&
          integrated->pm4Packets == 11 && integrated->pm4Draws == 1,
          "M24 metadata passed to M25 shader decode");
    check(integrated->pixelShaderAddress == 0x100000 &&
          integrated->buffer.baseAddress == 0x400000,
          "integrated shader and descriptor addresses");
    check(integrated->shaderReadOnly && integrated->descriptorReadOnly,
          "integrated fixture is read-only");
    MisakiGCN25Instruction out[16]{};
    MisakiGCN25Report report{};
    check(misaki_gcn25_diagnostic(out, 16, &report) == 0, "C bridge diagnostic runs");
    check(report.abi_version == 1 && report.instruction_count == 8 &&
          report.shader_words == 9 && report.pm4_packets == 11,
          "C bridge exposes correct trace counts");
    check(report.scalar_instructions == 5 && report.vector_instructions == 3 &&
          report.literal_words == 1 && report.terminated == 1,
          "C ABI GCN instruction family metadata");
    check(report.shader_read_only && report.descriptor_read_only &&
          report.descriptor_supported && report.descriptor_stride == 16 &&
          report.descriptor_records == 64 && report.descriptor_data_format == 14,
          "C ABI descriptor and memory protections");
    check(report.shader_address == 0x100000 && report.descriptor_base == 0x400000 &&
          report.checksum == decoded->checksum,
          "C ABI addresses and shader hash");
    check(out[0].encoding == 3 && out[3].encoding == 6 &&
          out[5].has_literal == 1 && out[5].literal == 0x3F800000,
          "C ABI instruction fields");
    out[0].opcode = 0x12345678;
    check(misaki_gcn25_diagnostic(out, 2, &report) == -3,
          "reject insufficient C buffer capacity");
    check(out[0].opcode == 0x12345678 && report.instruction_count == 0,
          "never partially write output on error");
    check(misaki_gcn25_diagnostic(nullptr, 0, &report) == -3,
          "reject missing instruction storage");
    check(misaki_gcn25_diagnostic(nullptr, 10, &report) == -1,
          "reject null output with nonzero capacity");
    check(misaki_gcn25_diagnostic(out, 10, nullptr) == -1, "reject null report");
    uint32_t seed = 0xCAB005E5u;
    for (int i = 0; i < 600; ++i) {
        std::vector<uint32_t> noise;
        for (int j = 0; j < (i % 24) + 1; ++j) {
            seed ^= seed << 13; seed ^= seed >> 17; seed ^= seed << 5;
            noise.push_back(seed);
        }
        auto attempt = decodeGCNShaderWords(noise.data(), noise.size(), 0x100000);
        check(!attempt || (attempt->ended && attempt->instructions.size() <= 24),
              "fuzz shader remains bounded and terminates");
    }
    std::cout << "PASS: " << checks << " AMD GCN ISA/descriptor assertions\n";
}
