#include "../Include/MisakiMUBUFBridge.h"
#include "../Include/GuestMUBUF.hpp"

#include <cstring>

extern "C" int32_t misaki_mubuf29_plan(
    char *metalSource, size_t sourceCapacity,
    uint8_t *inputBytes, size_t inputCapacity,
    uint32_t *initial, size_t initialCapacity,
    uint32_t *expected, size_t expectedCapacity,
    MisakiMUBUF29Report *report) {
    if (!report || (!metalSource && sourceCapacity) ||
        (!inputBytes && inputCapacity) || (!initial && initialCapacity) ||
        (!expected && expectedCapacity)) return -1;
    *report = MisakiMUBUF29Report{};
    try {
        const auto plan = misaki::runMUBUF29Diagnostic();
        if (!plan) return -2;
        const auto size = plan->metalSource.size() + 1;
        if (!metalSource || sourceCapacity < size || !inputBytes ||
            inputCapacity < plan->inputBytes.size() || !initial ||
            initialCapacity < plan->initial.size() || !expected ||
            expectedCapacity < plan->expected.size()) return -3;
        MisakiMUBUF29Report staged{};
        staged.abi_version = 1;
        staged.decoded_instructions = static_cast<uint32_t>(plan->decoded.instructions.size());
        staged.load_opcode = static_cast<uint32_t>(plan->decoded.instructions[0].opcode);
        staged.store_opcode = static_cast<uint32_t>(plan->decoded.instructions[1].opcode);
        staged.first_vaddr = plan->decoded.instructions[0].vaddr;
        staged.first_vdata = plan->decoded.instructions[0].vdata;
        staged.input_srsrc = plan->decoded.instructions[0].srsrc;
        staged.output_srsrc = plan->decoded.instructions[1].srsrc;
        staged.record_index = plan->recordIndex;
        staged.loaded_word = plan->loaded;
        staged.stored_word = plan->stored;
        staged.source_bytes = static_cast<uint32_t>(size);
        staged.input_bytes = static_cast<uint32_t>(plan->inputBytes.size());
        staged.output_words = static_cast<uint32_t>(plan->expected.size());
        staged.code_read_only = plan->codeReadOnly ? 1u : 0u;
        staged.descriptors_read_only = plan->descriptorsReadOnly ? 1u : 0u;
        staged.input_read_only = plan->sourceReadOnly ? 1u : 0u;
        staged.output_writable = plan->outputWritable ? 1u : 0u;
        staged.input_base = plan->inputBase;
        staged.output_base = plan->outputBase;
        staged.instruction_checksum = plan->decoded.checksum;
        staged.metal_source_hash = plan->shaderSourceHash;
        staged.expected_output_hash = plan->resultHash;
        std::memcpy(metalSource, plan->metalSource.c_str(), size);
        std::memcpy(inputBytes, plan->inputBytes.data(), plan->inputBytes.size());
        std::memcpy(initial, plan->initial.data(), plan->initial.size() * sizeof(uint32_t));
        std::memcpy(expected, plan->expected.data(), plan->expected.size() * sizeof(uint32_t));
        *report = staged;
        return 0;
    } catch (...) { return -4; }
}
