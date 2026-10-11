#include "GuestMetalIR.hpp"
#include "MisakiMetalIRBridge.h"
#include <array>
#include <cstdlib>
#include <iostream>
#include <string>

namespace {
int checks = 0;
void check(bool ok, const char *label) {
    ++checks;
    if (!ok) { std::cerr << "FAIL: " << label << '\n'; std::exit(1); }
}
}
int main() {
    using namespace misaki;
    const auto previous = runShaderIRDiagnostic();
    check(bool(previous), "M26 shader IR available");
    auto ir = previous->program;
    ShaderIRInputs inputs;
    inputs.vector[1] = 0x40000000u;
    std::string reason;
    const auto metal = translateShaderIRToMetal(ir, inputs, &reason);
    check(metal && reason.empty(), "translate GCN IR to MSL");
    check(metal->source.find("kernel void misaki_ir27") != std::string::npos, "kernel entry");
    check(metal->source.find("[[thread_position_in_grid]]") != std::string::npos, "compute work item");
    check(metal->source.find("uint s[102]") != std::string::npos &&
          metal->source.find("uint v[256]") != std::string::npos, "separate register banks");
    check(metal->source.find("v[1] = 0x40000000u;") != std::string::npos, "initial vector input");
    check(metal->source.find("s[3] = (s[1] + s[2]);") != std::string::npos, "scalar add");
    check(metal->source.find("as_type<float>") != std::string::npos &&
          metal->source.find("as_type<uint>") != std::string::npos, "float bits translated");
    check(metal->source.find("outValues[5] = v[2];") != std::string::npos, "result buffer output");
    check(metal->emittedALU == 6 && metal->sourceBytes == metal->source.size() + 1,
          "six translated moves/ALU operations");
    check(metal->expected == std::array<std::uint32_t, 6>{{7,7,14,0x40000000u,0x40800000u,0x40800000u}},
          "software reference outputs");
    check(metal->sourceHash != 0 && metal->irChecksum == ir.irChecksum &&
          metal->softwareResultChecksum == previous->execution.resultChecksum,
          "fingerprints tied to reference");
    const auto again = translateShaderIRToMetal(ir, inputs);
    check(again && again->source == metal->source && again->sourceHash == metal->sourceHash,
          "deterministic generated Metal source");
    inputs.vector[1] = 0x40400000u;
    const auto changed = translateShaderIRToMetal(ir, inputs);
    check(changed && changed->expected[5] == 0x40C00000u &&
          changed->sourceHash != metal->sourceHash && changed->irChecksum == metal->irChecksum,
          "changed input affects GPU source and output, not IR checksum");
    inputs.vector[1] = 0x40000000u;
    auto mutated = ir;
    mutated.instructions[2].op = ShaderIROp::scalarSub;
    const auto sub = translateShaderIRToMetal(mutated, inputs);
    check(sub && sub->expected[2] == 0 && sub->source.find("s[3] = (s[1] - s[2]);") != std::string::npos,
          "scalar subtraction MSL");
    mutated = ir; mutated.instructions[2].op = ShaderIROp::scalarAnd;
    const auto bitwise = translateShaderIRToMetal(mutated, inputs);
    check(bitwise && bitwise->expected[2] == 7 &&
          bitwise->source.find("s[3] = (s[1] & s[2]);") != std::string::npos, "scalar AND MSL");
    mutated = ir; mutated.instructions[1].op = ShaderIROp::scalarNot;
    const auto invert = translateShaderIRToMetal(mutated, inputs);
    check(invert && invert->expected[1] == ~7u &&
          invert->source.find("s[2] = (~s[1]);") != std::string::npos, "scalar NOT MSL");
    mutated = ir; mutated.instructions[4].op = ShaderIROp::vectorMulF32;
    const auto mul = translateShaderIRToMetal(mutated, inputs);
    check(mul && mul->expected[4] == 0x40800000u &&
          mul->source.find(" * as_type<float>") != std::string::npos, "vector multiply MSL");
    mutated = ir; mutated.instructions[0].a.value = 0xFFFFFFFFu;
    const auto full = translateShaderIRToMetal(mutated, inputs);
    check(full && full->source.find("0xFFFFFFFFu") != std::string::npos,
          "unsigned bit pattern literal intact");
    mutated = ir; mutated.instructions[1].a.kind = ShaderIRSourceKind::none;
    check(!translateShaderIRToMetal(mutated, inputs, &reason) && !reason.empty(),
          "missing operand rejected");
    mutated = ir; mutated.instructions[2].b.value = 102;
    check(!translateShaderIRToMetal(mutated, inputs), "out-of-range scalar operand");
    mutated = ir; mutated.instructions[5].b.value = 256;
    check(!translateShaderIRToMetal(mutated, inputs), "out-of-range vector operand");
    mutated = ir; mutated.instructions[5].op = static_cast<ShaderIROp>(0xBABEu);
    check(!translateShaderIRToMetal(mutated, inputs), "unsupported opcode");
    mutated = ir; mutated.instructions[3].destination = 256;
    check(!translateShaderIRToMetal(mutated, inputs), "out-of-range vector destination");
    mutated = ir; mutated.instructions[1].destination = 102;
    check(!translateShaderIRToMetal(mutated, inputs), "out-of-range scalar destination");
    mutated = ir; mutated.instructions.back().op = ShaderIROp::nop;
    check(!translateShaderIRToMetal(mutated, inputs), "missing termination");
    mutated = ir; mutated.instructions[2].op = ShaderIROp::end;
    check(!translateShaderIRToMetal(mutated, inputs), "early termination");
    mutated = ir; mutated.instructions.clear();
    check(!translateShaderIRToMetal(mutated, inputs), "empty program");
    mutated = ir; mutated.sourceChecksum = 0;
    check(!translateShaderIRToMetal(mutated, inputs), "missing source hash");
    mutated = ir; mutated.irChecksum = 0;
    check(!translateShaderIRToMetal(mutated, inputs), "missing IR checksum");
    ShaderIRInputs bad = inputs;
    bad.vector[1] = 0x7F800000u;
    check(!translateShaderIRToMetal(ir, bad), "reject infinity before GPU dispatch");
    bad.vector[1] = 0x7F7FFFFFu;
    check(!translateShaderIRToMetal(ir, bad), "reject floating overflow before GPU");

    const auto integrated = runMetalIRDiagnostic();
    check(integrated && integrated->sourceHash == metal->sourceHash,
          "integrated PM4 to ISA to IR to MSL consistent");
    char output[32768]{};
    MisakiMetalIR27Report report{};
    check(misaki_metal_ir27_shader(output, sizeof(output), &report) == 0,
          "C interface exports standalone shader source");
    check(std::string(output) == metal->source, "C source identical to C++ source");
    check(report.abi_version == 1 && report.ir_instructions == 8 &&
          report.alu_statements == 6 && report.source_bytes == metal->sourceBytes,
          "C bridge counts");
    check(report.ir_checksum == metal->irChecksum &&
          report.metal_source_hash == metal->sourceHash &&
          report.software_result_checksum == metal->softwareResultChecksum,
          "C bridge hashes");
    check(report.sgpr1 == 7 && report.sgpr2 == 7 && report.sgpr3 == 14 &&
          report.vgpr0_bits == 0x40000000u &&
          report.vgpr1_bits == 0x40800000u && report.vgpr2_bits == 0x40800000u,
          "C bridge output register bit patterns");
    output[0] = 'Q';
    check(misaki_metal_ir27_shader(output, 1, &report) == -3,
          "insufficient caller buffer rejected");
    check(output[0] == 'Q' && report.abi_version == 0,
          "no partial writes on insufficient buffer");
    check(misaki_metal_ir27_shader(nullptr, 0, &report) == -3,
          "null and zero-capacity safely rejected");
    check(misaki_metal_ir27_shader(nullptr, 10, &report) == -1,
          "null buffer with claimed capacity rejected");
    check(misaki_metal_ir27_shader(output, sizeof(output), nullptr) == -1,
          "null report rejected");

    for (unsigned i = 0; i < 400; ++i) {
        mutated = ir;
        mutated.instructions[i % mutated.instructions.size()].destination = i * 47u;
        const auto candidate = translateShaderIRToMetal(mutated, inputs);
        check(!candidate || candidate->sourceBytes <= 32768,
              "mutated input cannot exceed bounded shader source budget");
    }
    std::cout << "PASS: " << checks << " Metal source translation assertions\n";
}
