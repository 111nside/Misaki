#include "../Include/MisakiPM423Bridge.h"
#include "../Include/GuestPM4Indirect.hpp"

extern "C" int32_t misaki_pm423_diagnostic(MisakiPM423Packet *output,
                                            size_t capacity,
                                            MisakiPM423Report *report) {
    if (!report || (!output && capacity != 0)) return -1;
    *report = MisakiPM423Report{};
    try {
        misaki::GuestMemory memory;
        const auto fixture = misaki::makePM4IndirectFixture();
        if (!misaki::installPM4IndirectFixture(memory, fixture)) return -2;
        const auto trace = misaki::decodePM4Indirect(memory, fixture.root,
                                                      fixture.rootWords.size());
        if (!trace) return -2;
        if (!output || capacity < trace->packets.size()) return -3;
        // Preflight the full trace before copying into caller's output.
        const auto &state = trace->state;
        if (!state.hasRenderTargetMetadata() || trace->indirectBuffers != 2 ||
            trace->drawAuto != 1 || trace->eventWrites != 1) return -2;
        for (std::size_t i = 0; i < trace->packets.size(); ++i) {
            const auto &p = trace->packets[i];
            output[i] = {p.guestAddress, p.type, p.opcode, p.depth, p.bodyWords};
        }
        report->abi_version = 1;
        report->packet_count = static_cast<uint32_t>(trace->packets.size());
        report->indirect_buffers = trace->indirectBuffers;
        report->indirect_const_buffers = trace->indirectConstBuffers;
        report->maximum_depth = trace->maxDepth;
        report->guest_words = trace->totalWords;
        report->register_writes = trace->registerWrites;
        report->draw_auto_packets = trace->drawAuto;
        report->event_writes = trace->eventWrites;
        report->observed_register_mask = state.observedMask;
        report->render_target_metadata_observed = state.hasRenderTargetMetadata() ? 1u : 0u;
        report->color0_base = state.color0Base;
        report->color0_pitch = state.color0Pitch;
        report->color0_info = state.color0Info;
        report->color_target_mask = state.targetMask;
        report->scissor_tl = state.scissorTL;
        report->scissor_br = state.scissorBR;
        report->pixel_shader_low = state.pixelShaderLow;
        report->pixel_shader_high = state.pixelShaderHigh;
        report->checksum = trace->checksum;
        return 0;
    } catch (...) {
        return -4;
    }
}
