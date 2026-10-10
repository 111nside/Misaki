#include "../Include/MisakiGCN25Bridge.h"
#include "../Include/GuestGCNISA.hpp"

extern "C" int32_t misaki_gcn25_diagnostic(MisakiGCN25Instruction *instructions,
                                              size_t capacity, MisakiGCN25Report *report) {
    if (!report || (!instructions && capacity != 0)) return -1;
    *report = MisakiGCN25Report{};
    try {
        const auto run = misaki::runGCNShaderDiagnostic();
        if (!run) return -2;
        const auto &shader = run->shader;
        if (shader.instructions.size() > capacity || !instructions) return -3;
        for (std::size_t i = 0; i < shader.instructions.size(); ++i) {
            const auto &ins = shader.instructions[i];
            instructions[i] = {ins.guestAddress, ins.wordOffset,
                               static_cast<uint32_t>(ins.encoding), ins.opcode,
                               ins.dst, ins.src0, ins.src1, ins.literal,
                               ins.wordCount, ins.hasLiteral ? 1u : 0u};
        }
        report->abi_version = 1;
        report->pm4_packets = run->pm4Packets;
        report->pm4_draws = run->pm4Draws;
        report->instruction_count = static_cast<uint32_t>(shader.instructions.size());
        report->scalar_instructions = shader.scalarInstructions;
        report->vector_instructions = shader.vectorInstructions;
        report->literal_words = shader.literalWords;
        report->shader_words = shader.totalWords;
        report->terminated = shader.ended ? 1u : 0u;
        report->shader_read_only = run->shaderReadOnly ? 1u : 0u;
        report->descriptor_read_only = run->descriptorReadOnly ? 1u : 0u;
        report->descriptor_supported = run->buffer.supportedLinearMetadata ? 1u : 0u;
        report->descriptor_stride = run->buffer.strideBytes;
        report->descriptor_records = run->buffer.records;
        report->descriptor_data_format = run->buffer.dataFormat;
        report->descriptor_numeric_format = run->buffer.numericFormat;
        report->descriptor_swizzled = run->buffer.swizzleEnabled ? 1u : 0u;
        report->shader_address = run->pixelShaderAddress;
        report->descriptor_base = run->buffer.baseAddress;
        report->checksum = shader.checksum;
        return 0;
    } catch (...) { return -4; }
}
