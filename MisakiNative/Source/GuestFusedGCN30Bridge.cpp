#include "../Include/MisakiFusedGCN30Bridge.h"
#include "../Include/GuestFusedGCN30.hpp"

#include <cstring>
#include <limits>

extern "C" int32_t misaki_gcn30_plan(char *source, size_t sourceCapacity,
    uint8_t *input, size_t inputCapacity,
    uint32_t *initial, size_t initialCapacity,
    uint32_t *expected, size_t expectedCapacity,
    uint32_t *registers, size_t registerCapacity,
    MisakiFusedGCN30Report *report) {
    if (!report || !source || !input || !initial || !expected || !registers) return -1;
    *report = MisakiFusedGCN30Report{};
    try {
        const auto run = misaki::runFusedGCN30Diagnostic();
        if (!run) return -2;
        if (sourceCapacity < run->metalSource.size() + 1 ||
            inputCapacity < run->inputBytes.size() ||
            initialCapacity < run->initial.size() ||
            expectedCapacity < run->expected.output.size() ||
            registerCapacity < run->expected.registers.size()) return -3;
        if (run->metalSource.size() + 1 > std::numeric_limits<uint32_t>::max()) return -2;
        // Transactional C ABI: all buffers validated before first write.
        std::memcpy(source, run->metalSource.c_str(), run->metalSource.size() + 1);
        std::memcpy(input, run->inputBytes.data(), run->inputBytes.size());
        std::memcpy(initial, run->initial.data(), sizeof(run->initial));
        std::memcpy(expected, run->expected.output.data(), sizeof(run->expected.output));
        std::memcpy(registers, run->expected.registers.data(), sizeof(run->expected.registers));
        report->abi_version = 1;
        report->instructions = static_cast<uint32_t>(run->decoded.steps.size());
        report->code_words = run->decoded.codeWords;
        report->alu_operations = run->decoded.aluOperations;
        report->buffer_loads = run->decoded.bufferLoads;
        report->buffer_stores = run->decoded.bufferStores;
        report->executed = run->expected.executed;
        report->record_index = run->expected.index;
        report->loaded = run->expected.loaded;
        report->stored = run->expected.stored;
        report->source_bytes = static_cast<uint32_t>(run->metalSource.size() + 1);
        report->input_bytes = static_cast<uint32_t>(run->inputBytes.size());
        report->output_words = static_cast<uint32_t>(run->expected.output.size());
        report->register_words = static_cast<uint32_t>(run->expected.registers.size());
        report->code_read_only = run->codeReadOnly;
        report->descriptors_read_only = run->descriptorsReadOnly;
        report->input_read_only = run->inputReadOnly;
        report->output_writable = run->outputWritable;
        report->input_base = run->inputBase;
        report->output_base = run->outputBase;
        report->instruction_checksum = run->decoded.checksum;
        report->metal_source_hash = run->metalSourceHash;
        report->expected_output_hash = run->expectedHash;
        return 0;
    } catch (...) { return -4; }
}
