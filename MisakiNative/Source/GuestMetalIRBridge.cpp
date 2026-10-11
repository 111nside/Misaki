#include "../Include/MisakiMetalIRBridge.h"
#include "../Include/GuestMetalIR.hpp"

#include <cstring>

extern "C" int32_t misaki_metal_ir27_shader(char *output, size_t capacity,
                                                MisakiMetalIR27Report *report) {
    if (!report || (!output && capacity != 0)) return -1;
    *report = MisakiMetalIR27Report{};
    try {
        const auto t = misaki::runMetalIRDiagnostic();
        if (!t) return -2;
        if (!output || capacity < t->sourceBytes) return -3;
        MisakiMetalIR27Report staged{};
        staged.abi_version = 1;
        staged.ir_instructions = 8;
        staged.alu_statements = t->emittedALU;
        staged.source_bytes = t->sourceBytes;
        staged.sgpr1 = t->expected[0];
        staged.sgpr2 = t->expected[1];
        staged.sgpr3 = t->expected[2];
        staged.vgpr0_bits = t->expected[3];
        staged.vgpr1_bits = t->expected[4];
        staged.vgpr2_bits = t->expected[5];
        staged.ir_checksum = t->irChecksum;
        staged.metal_source_hash = t->sourceHash;
        staged.software_result_checksum = t->softwareResultChecksum;
        std::memcpy(output, t->source.c_str(), t->sourceBytes);
        *report = staged;
        return 0;
    } catch (...) { return -4; }
}
