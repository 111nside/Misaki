#include "../Include/MisakiMetalResourcesBridge.h"
#include "../Include/GuestMetalResources.hpp"

#include <cstring>

extern "C" int32_t misaki_metal_resource28_plan(
    char *source, size_t sourceCapacity,
    uint8_t *inputBytes, size_t inputCapacity,
    uint32_t *initial, size_t initialCapacity,
    uint32_t *expected, size_t expectedCapacity,
    MisakiMetalResource28Report *report) {
    if (!report || (!source && sourceCapacity) || (!inputBytes && inputCapacity) ||
        (!initial && initialCapacity) || (!expected && expectedCapacity)) return -1;
    *report = MisakiMetalResource28Report{};
    try {
        const auto plan = misaki::runMetalResource28Diagnostic();
        if (!plan) return -2;
        const std::size_t bytes = plan->resourceMSL.size() + 1;
        if (!source || sourceCapacity < bytes || !inputBytes ||
            inputCapacity < plan->guestBytes.size() || !initial ||
            initialCapacity < plan->outputInitial.size() || !expected ||
            expectedCapacity < plan->outputExpected.size()) return -3;
        MisakiMetalResource28Report staged{};
        staged.abi_version = 1;
        staged.source_bytes = static_cast<uint32_t>(bytes);
        staged.input_bytes = static_cast<uint32_t>(plan->guestBytes.size());
        staged.output_words = static_cast<uint32_t>(plan->outputInitial.size());
        staged.resource_ops = 3;
        staged.descriptor_stride = plan->descriptorStride;
        staged.descriptor_records = plan->descriptorRecords;
        staged.read_index = plan->readIndex;
        staged.write_index = plan->writeIndex;
        staged.loaded_word = plan->loadedWord;
        staged.stored_word = plan->storedWord;
        staged.guest_input_read_only = plan->guestInputReadOnly ? 1u : 0u;
        staged.guest_output_writable = plan->guestOutputWritable ? 1u : 0u;
        staged.descriptor_base = plan->descriptorBase;
        staged.ir_checksum = plan->irChecksum;
        staged.metal_source_hash = plan->sourceHash;
        staged.output_initial_hash = plan->initialHash;
        staged.output_expected_hash = plan->expectedHash;
        std::memcpy(source, plan->resourceMSL.c_str(), bytes);
        std::memcpy(inputBytes, plan->guestBytes.data(), plan->guestBytes.size());
        std::memcpy(initial, plan->outputInitial.data(), plan->outputInitial.size() * sizeof(uint32_t));
        std::memcpy(expected, plan->outputExpected.data(), plan->outputExpected.size() * sizeof(uint32_t));
        *report = staged;
        return 0;
    } catch (...) { return -4; }
}
