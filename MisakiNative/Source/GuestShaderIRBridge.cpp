#include "../Include/MisakiShaderIRBridge.h"
#include "../Include/GuestShaderIR.hpp"

extern "C" int32_t misaki_shader_ir_demo(MisakiShaderIRInstruction *output,
                                            size_t capacity, MisakiShaderIRReport *report) {
    if (!report || (!output && capacity != 0)) return -1;
    *report = MisakiShaderIRReport{};
    try {
        const auto result = misaki::runShaderIRDiagnostic();
        if (!result) return -2;
        const auto &ir = result->program;
        const auto &exec = result->execution;
        if (ir.instructions.size() > capacity || !output) return -3;
        if (result->pm4Packets != 11 || result->pm4Draws != 1 ||
            ir.instructions.size() != 8 || ir.scalarInstructions != 5 ||
            ir.vectorInstructions != 3 || exec.executedInstructions != 8 ||
            exec.scalarWrites != 3 || exec.vectorWrites != 3 || !exec.terminated ||
            exec.finalRegisters.scalar[1] != 7 || exec.finalRegisters.scalar[2] != 7 ||
            exec.finalRegisters.scalar[3] != 14 ||
            exec.finalRegisters.vector[0] != 0x40000000u ||
            exec.finalRegisters.vector[1] != 0x40800000u ||
            exec.finalRegisters.vector[2] != 0x40800000u ||
            result->shaderAddress != 0x100000u ||
            !result->sourceReadOnly || !result->descriptorReadOnly)
            return -2;
        for (std::size_t i = 0; i < ir.instructions.size(); ++i) {
            const auto &ins = ir.instructions[i];
            output[i] = {ins.guestAddress, ins.wordOffset,
                         static_cast<uint32_t>(ins.op), ins.destination,
                         static_cast<uint32_t>(ins.a.kind), ins.a.value,
                         static_cast<uint32_t>(ins.b.kind), ins.b.value};
        }
        report->abi_version = 1;
        report->pm4_packets = result->pm4Packets;
        report->pm4_draws = result->pm4Draws;
        report->ir_instructions = static_cast<uint32_t>(ir.instructions.size());
        report->scalar_instructions = ir.scalarInstructions;
        report->vector_instructions = ir.vectorInstructions;
        report->executed_instructions = exec.executedInstructions;
        report->scalar_writes = exec.scalarWrites;
        report->vector_writes = exec.vectorWrites;
        report->terminated = exec.terminated ? 1u : 0u;
        report->source_read_only = result->sourceReadOnly ? 1u : 0u;
        report->descriptor_read_only = result->descriptorReadOnly ? 1u : 0u;
        report->sgpr1 = exec.finalRegisters.scalar[1];
        report->sgpr2 = exec.finalRegisters.scalar[2];
        report->sgpr3 = exec.finalRegisters.scalar[3];
        report->vgpr0_bits = exec.finalRegisters.vector[0];
        report->vgpr1_bits = exec.finalRegisters.vector[1];
        report->vgpr2_bits = exec.finalRegisters.vector[2];
        report->shader_address = result->shaderAddress;
        report->source_checksum = ir.sourceChecksum;
        report->ir_checksum = ir.irChecksum;
        report->result_checksum = exec.resultChecksum;
        return 0;
    } catch (...) { return -4; }
}
