#include "../Include/MisakiPM424Bridge.h"
#include "../Include/GuestPM4State.hpp"

extern "C" int32_t misaki_pm424_diagnostic(MisakiPM424Draw *output,
                                             size_t capacity,
                                             MisakiPM424Report *report) {
    if (!report || (!output && capacity != 0)) return -1;
    *report = MisakiPM424Report{};
    try {
        const auto state = misaki::runPM4StateDiagnostic();
        if (!state) return -2;
        if (state->draws.size() > capacity ||
            (output == nullptr && !state->draws.empty())) return -3;
        for (std::size_t i = 0; i < state->draws.size(); ++i) {
            const auto &d = state->draws[i];
            output[i] = {d.packetAddress, d.colorAddress, d.pixelShaderAddress,
                         d.packetIndex, d.depth, d.vertexCount,
                         d.scissorX, d.scissorY, d.scissorWidth, d.scissorHeight,
                         d.pitchTileMax, d.colorFormatField, d.targetMask,
                         d.observedMask, static_cast<std::uint32_t>(d.status)};
        }
        report->abi_version = 1;
        report->packets = state->packets;
        report->indirect_buffers = state->indirectBuffers;
        report->register_writes = state->registerWrites;
        report->draws = static_cast<std::uint32_t>(state->draws.size());
        report->ready_draws = state->readyDraws;
        report->rejected_draws = state->rejectedDraws;
        report->checksum = state->checksum;
        return 0;
    } catch (...) {
        return -4;
    }
}
