#include "../Include/MisakiCoreBridge.h"
#include "../Include/MisakiCore.hpp"

extern "C" {

const char *misaki_core_version(void) { return "0.9.0-native-prototype"; }

int32_t misaki_core_run_diagnostic(MisakiCoreReport *report) {
    if (!report) return -1;
    *report = MisakiCoreReport{};
    try {
        const auto result = misaki::runDiagnostic();
        if (!result) return -2;
        report->abi_version = 1;
        report->elf_type = result->elf.type;
        report->load_segments = result->elf.loadSegments;
        report->registered_modules = result->loadedModules;
        report->resolved_imports = result->importsResolved;
        report->import_is_read_only = result->importReadOnly ? 1u : 0u;
        report->linked_guest_address = result->linkedAddress;
        return 0;
    } catch (...) {
        return -3;
    }
}

int32_t misaki_core_inspect_elf(const uint8_t *bytes, size_t length,
                               uint32_t *type, uint32_t *load_segment_count) {
    if (!type || !load_segment_count) return -1;
    *type = 0;
    *load_segment_count = 0;
    try {
        const auto info = misaki::inspectELF64(bytes, length);
        if (!info) return -2;
        *type = info->type;
        *load_segment_count = info->loadSegments;
        return 0;
    } catch (...) {
        return -3;
    }
}
} // extern "C"
