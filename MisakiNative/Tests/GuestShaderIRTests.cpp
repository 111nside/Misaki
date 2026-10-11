#include "GuestShaderIR.hpp"
#include "MisakiShaderIRBridge.h"

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
    if (!ok) { std::cerr << "FAIL: " << message << "\n"; std::exit(EXIT_FAILURE); }
}
std::uint32_t sop1(std::uint32_t d, std::uint32_t op, std::uint32_t s) {
    return 0xBE800000u | (d << 16) | (op << 8) | s;
}
std::uint32_t sop2(std::uint32_t d, std::uint32_t op, std::uint32_t s0, std::uint32_t s1) {
    return 0x80000000u | (op << 23) | (d << 16) | (s1 << 8) | s0;
}
std::uint32_t sopk(std::uint32_t d, std::uint32_t imm16) {
    return 0xB0000000u | (d << 16) | imm16;
}
std::uint32_t vop1(std::uint32_t d, std::uint32_t s0) {
    return 0x7E000000u | (d << 17) | (1u << 9) | s0;
}
std::uint32_t vop2(std::uint32_t d, std::uint32_t op, std::uint32_t s0, std::uint32_t s1) {
    return (op << 25) | (d << 17) | (s1 << 9) | s0;
}
std::optional<misaki::ShaderIRProgram> lower(const std::vector<std::uint32_t> &words,
                                              std::string *e = nullptr) {
    auto parsed = misaki::decodeGCNShaderWords(words.data(), words.size(), 0x100000, e);
    if (!parsed) return std::nullopt;
    return misaki::lowerGCNToShaderIR(*parsed, e);
}
void reject(const std::vector<std::uint32_t> &words, const char *message) {
    std::string error;
    auto ir = lower(words, &error);
    check(!ir, message);
    check(!error.empty(), "unsupported GCN shader provides diagnostic reason");
}
}

int main() {
    using namespace misaki;
    const auto fixture = makeGCNShaderFixture();
    const auto decoded = decodeGCNShaderWords(fixture.data(), fixture.size(), 0x100000);
    check(bool(decoded), "shader decoded by previous milestone");
    std::string error;
    const auto ir = lowerGCNToShaderIR(*decoded, &error);
    check(bool(ir) && error.empty(), "GCN instructions lowered into IR");
    check(ir->instructions.size() == 8, "eight IR operations");
    check(ir->scalarInstructions == 5 && ir->vectorInstructions == 3, "scalar vector counts");
    check(ir->sourceChecksum == decoded->checksum && ir->irChecksum != 0,
          "checksummed lowering tied to decoded shader");
    check(ir->instructions[0].op == ShaderIROp::scalarMovImmediate &&
          ir->instructions[0].a.kind == ShaderIRSourceKind::immediateBits &&
          ir->instructions[0].a.value == 7, "SOPK -> typed immediate");
    check(ir->instructions[1].op == ShaderIROp::scalarMov &&
          ir->instructions[1].a.kind == ShaderIRSourceKind::scalarRegister &&
          ir->instructions[1].a.value == 1, "SOP1 -> SGPR source");
    check(ir->instructions[2].op == ShaderIROp::scalarAdd &&
          ir->instructions[2].a.value == 1 && ir->instructions[2].b.value == 2,
          "SOP2 -> two SGPR sources");
    check(ir->instructions[3].op == ShaderIROp::vectorMov &&
          ir->instructions[3].a.kind == ShaderIRSourceKind::vectorRegister &&
          ir->instructions[3].a.value == 1, "VOP1 -> VGPR source");
    check(ir->instructions[4].op == ShaderIROp::vectorAddF32 &&
          ir->instructions[4].a.value == 0 && ir->instructions[4].b.value == 0,
          "VOP2 -> two VGPR sources");
    check(ir->instructions[5].op == ShaderIROp::vectorMulF32 &&
          ir->instructions[5].a.kind == ShaderIRSourceKind::immediateBits &&
          ir->instructions[5].a.value == 0x3F800000u &&
          ir->instructions[5].b.kind == ShaderIRSourceKind::vectorRegister &&
          ir->instructions[5].b.value == 1, "literal source in floating VOP2");
    check(ir->instructions[6].op == ShaderIROp::nop &&
          ir->instructions[7].op == ShaderIROp::end, "explicit shader termination");
    check(ir->instructions[7].wordOffset == 8, "extra literal preserved in offsets");

    ShaderIRInputs inputs;
    inputs.vector[1] = 0x40000000u; // 2.0f
    auto exec = evaluateShaderIR(*ir, inputs, &error);
    check(bool(exec) && error.empty(), "evaluate bounded one-lane shader program");
    check(exec->terminated && exec->executedInstructions == 8,
          "includes NOP and END in execution count");
    check(exec->scalarWrites == 3 && exec->vectorWrites == 3, "six typed register writes");
    check(exec->finalRegisters.scalar[1] == 7 && exec->finalRegisters.scalar[2] == 7 &&
          exec->finalRegisters.scalar[3] == 14, "S_MOVK + S_MOV + S_ADD result");
    check(exec->finalRegisters.vector[0] == 0x40000000u &&
          exec->finalRegisters.vector[1] == 0x40800000u &&
          exec->finalRegisters.vector[2] == 0x40800000u,
          "V_MOV + V_ADD + V_MUL gives 2,4,4 floats");
    check(exec->resultChecksum != 0 &&
          exec->resultChecksum == evaluateShaderIR(*ir, inputs)->resultChecksum,
          "deterministic IR execution checksum");
    check(inputs.vector[0] == 0 && inputs.vector[1] == 0x40000000u,
          "evaluator does not mutate caller inputs");
    inputs.vector[1] = 0x40400000u; // 3.0f
    const auto other = evaluateShaderIR(*ir, inputs);
    check(other && other->finalRegisters.vector[2] == 0x40C00000u,
          "different input yields six float in final VGPR");
    check(other->resultChecksum != exec->resultChecksum,
          "different input changes result checksum");

    // Validate scalar inline constants, sign extension, integer wrapping,
    // bit operations, scalar/float source distinctions and negative values.
    const std::vector<std::uint32_t> integerOps = {
        sopk(1, 0xFFFF),                     // s1=-1
        sop1(2, 0, 193),                     // s2=-1 inline
        sop2(3, 0, 1, 128),                  // s3=s1+0 => -1
        sop2(4, 1, 1, 129),                  // s4=-1-1 => -2
        sop2(5, 12, 1, 193),                 // s5=-1&-1 => -1
        sop1(6, 4, 1),                       // s6=~(-1) => 0
        sop1(7, 0, 255), 0x12345678u,       // s7=literal
        sopk(8, 255),                       // immediate 255 is not literal marker!
        0xBF810000u
    };
    const auto ints = lower(integerOps, &error);
    check(bool(ints) && error.empty(), "lower scalar integer operations and SOPK 255");
    const auto runInts = evaluateShaderIR(*ints, ShaderIRInputs{});
    check(bool(runInts) && runInts->terminated, "execute scalar integer subset");
    check(runInts->finalRegisters.scalar[1] == 0xFFFFFFFFu &&
          runInts->finalRegisters.scalar[2] == 0xFFFFFFFFu &&
          runInts->finalRegisters.scalar[3] == 0xFFFFFFFFu &&
          runInts->finalRegisters.scalar[4] == 0xFFFFFFFEu &&
          runInts->finalRegisters.scalar[5] == 0xFFFFFFFFu &&
          runInts->finalRegisters.scalar[6] == 0 &&
          runInts->finalRegisters.scalar[7] == 0x12345678u &&
          runInts->finalRegisters.scalar[8] == 255,
          "typed scalar semantics and signed immediates correct");
    const std::vector<std::uint32_t> vectorOps = {
        vop1(1, 255), 0x40000000u,          // v1=2.0f literal
        vop1(2, 257),                       // v2=v1
        vop2(3, 1, 258, 1),                 // v3=v2+v1 => 4
        vop2(4, 5, 258, 3),                 // v4=v2*v3 => 8
        vop1(5, 129),                       // v5=uint bits 1 (NOT 1.0 float)
        0xBF810000u
    };
    const auto vir = lower(vectorOps);
    check(bool(vir), "lower vector VGPR and literal instructions");
    const auto vexec = evaluateShaderIR(*vir, ShaderIRInputs{});
    check(vexec && vexec->finalRegisters.vector[1] == 0x40000000u &&
          vexec->finalRegisters.vector[2] == 0x40000000u &&
          vexec->finalRegisters.vector[3] == 0x40800000u &&
          vexec->finalRegisters.vector[4] == 0x41000000u &&
          vexec->finalRegisters.vector[5] == 1,
          "float ALU and move bits semantics");
    const std::vector<std::uint32_t> nonfinite = {vop2(0, 1, 256, 0), 0xBF810000u};
    auto nir = lower(nonfinite);
    check(bool(nir), "valid program can receive bad float data");
    ShaderIRInputs badFloat;
    badFloat.vector[0] = 0x7F800000u;
    check(!evaluateShaderIR(*nir, badFloat, &error) && !error.empty(),
          "reject infinity as input, never silently produce unsafe result");
    badFloat.vector[0] = 0x7F7FFFFFu;
    check(!evaluateShaderIR(*nir, badFloat, &error) && !error.empty(),
          "reject floating overflow output");

    auto forged = *decoded;
    forged.instructions[2].wordOffset++;
    check(!lowerGCNToShaderIR(forged, &error) && !error.empty(),
          "reject out-of-order instruction offsets");
    forged = *decoded;
    forged.instructions[3].guestAddress += 4;
    check(!lowerGCNToShaderIR(forged), "reject inconsistent guest address");
    forged = *decoded;
    forged.instructions[5].wordCount = 1;
    check(!lowerGCNToShaderIR(forged), "reject missing literal word count");
    forged = *decoded;
    forged.instructions[7].opcode = 2;
    check(!lowerGCNToShaderIR(forged), "reject unsupported SOPP opcode");
    forged = *decoded;
    forged.instructions[7].src0 = 1;
    check(!lowerGCNToShaderIR(forged), "reject malformed END operand");
    forged = *decoded;
    forged.instructions[3].src0 = 512;
    check(!lowerGCNToShaderIR(forged), "reject invalid VGPR selector");
    forged = *decoded;
    forged.instructions[0].dst = 200;
    check(!lowerGCNToShaderIR(forged), "reject out-of-range SGPR");
    forged = *decoded;
    forged.instructions[1].src0 = 111;
    check(!lowerGCNToShaderIR(forged), "reject reserved scalar selector");
    forged = *decoded;
    forged.instructions[0].guestAddress = UINT64_MAX - 3;
    check(!lowerGCNToShaderIR(forged), "reject overflowing shader address");
    forged = *decoded;
    forged.ended = false;
    check(!lowerGCNToShaderIR(forged), "reject non-terminating decoded trace");
    forged = *decoded;
    forged.scalarInstructions = 0;
    check(!lowerGCNToShaderIR(forged), "reject falsified instruction counts");
    forged = *decoded;
    forged.instructions.clear();
    check(!lowerGCNToShaderIR(forged), "reject empty shader trace");

    auto corruptedIR = *ir;
    corruptedIR.instructions[2].a.kind = ShaderIRSourceKind::none;
    check(!evaluateShaderIR(corruptedIR, ShaderIRInputs{}, &error) && !error.empty(),
          "reject forged missing register operand");
    corruptedIR = *ir;
    corruptedIR.instructions[5].b.value = 256;
    check(!evaluateShaderIR(corruptedIR, ShaderIRInputs{}), "reject out-of-range VGPR");
    corruptedIR = *ir;
    corruptedIR.instructions[0].destination = 102;
    check(!evaluateShaderIR(corruptedIR, ShaderIRInputs{}), "reject out-of-range SGPR write");
    corruptedIR = *ir;
    corruptedIR.instructions[4].op = static_cast<ShaderIROp>(650);
    check(!evaluateShaderIR(corruptedIR, ShaderIRInputs{}), "reject unsupported forged IR op");
    corruptedIR = *ir;
    corruptedIR.instructions[4].op = ShaderIROp::end;
    check(!evaluateShaderIR(corruptedIR, ShaderIRInputs{}), "reject premature IR END");
    corruptedIR = *ir;
    corruptedIR.instructions.pop_back();
    check(!evaluateShaderIR(corruptedIR, ShaderIRInputs{}), "reject missing IR END");
    corruptedIR = *ir;
    corruptedIR.instructions.clear();
    check(!evaluateShaderIR(corruptedIR, ShaderIRInputs{}), "reject empty IR program");

    const auto integrated = runShaderIRDiagnostic();
    check(bool(integrated), "end to end PM4 -> GCN decode -> IR -> eval");
    check(integrated->sourceReadOnly && integrated->descriptorReadOnly,
          "respects read-only mapped shader and descriptor");
    check(integrated->pm4Packets == 11 && integrated->pm4Draws == 1 &&
          integrated->shaderAddress == 0x100000, "draw-state shader address integrated");
    check(integrated->program.irChecksum == ir->irChecksum &&
          integrated->execution.finalRegisters.vector[2] == 0x40800000u,
          "integrated IR equals standalone interpretation");

    MisakiShaderIRInstruction cIn[16]{};
    MisakiShaderIRReport cReport{};
    check(misaki_shader_ir_demo(cIn, 16, &cReport) == 0, "C bridge reports IR diagnostic");
    check(cReport.abi_version == 1 && cReport.ir_instructions == 8 &&
          cReport.scalar_instructions == 5 && cReport.vector_instructions == 3,
          "C ABI v1 and typed counts");
    check(cReport.pm4_packets == 11 && cReport.pm4_draws == 1 &&
          cReport.source_read_only == 1 && cReport.descriptor_read_only == 1,
          "C ABI exposes original draw and guest memory permissions");
    check(cReport.sgpr1 == 7 && cReport.sgpr2 == 7 && cReport.sgpr3 == 14 &&
          cReport.vgpr0_bits == 0x40000000u &&
          cReport.vgpr1_bits == 0x40800000u &&
          cReport.vgpr2_bits == 0x40800000u,
          "C ABI exposes final typed register values");
    check(cReport.source_checksum == decoded->checksum &&
          cReport.ir_checksum == ir->irChecksum &&
          cReport.result_checksum == integrated->execution.resultChecksum,
          "C/native trace, IR and result hashes agree");
    check(cIn[0].op == 1 && cIn[5].op == 9 && cIn[7].op == 11 &&
          cIn[5].src0_kind == 3 && cIn[5].src0_value == 0x3F800000u,
          "C bridge exports tagged IR operands");
    cIn[0].op = 650;
    check(misaki_shader_ir_demo(cIn, 1, &cReport) == -3,
          "short output rejected atomically");
    check(cIn[0].op == 650 && cReport.ir_instructions == 0,
          "no partial writes or success report on failure");
    check(misaki_shader_ir_demo(cIn, 16, nullptr) == -1, "reject null report");
    check(misaki_shader_ir_demo(nullptr, 16, &cReport) == -1,
          "reject null buffer when capacity nonzero");
    check(misaki_shader_ir_demo(nullptr, 0, &cReport) == -3,
          "zero-length buffer reports capacity failure");

    // Randomized valid operand selector smoke test, no unsafe host pointers.
    std::uint32_t seed = 0xCAB00D55u;
    for (unsigned i = 0; i < 256; ++i) {
        seed ^= seed << 13; seed ^= seed >> 17; seed ^= seed << 5;
        const std::uint32_t registerIndex = (seed % 100) + 1;
        const std::uint32_t constant = (seed >> 12) % 65;
        const std::vector<std::uint32_t> shader = {
            sop1(registerIndex, 0, 128 + constant),
            sop2(registerIndex, 0, registerIndex, 129),
            0xBF810000u
        };
        auto p = lower(shader);
        check(bool(p), "fuzz valid scalar source lowering");
        const auto x = evaluateShaderIR(*p, ShaderIRInputs{});
        check(x && x->finalRegisters.scalar[registerIndex] == constant + 1,
              "fuzz deterministic scalar ALU output");
    }
    std::cout << "PASS: " << checks << " native shader IR assertions\n";
}
