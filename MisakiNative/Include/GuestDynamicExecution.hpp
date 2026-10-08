#pragma once

#include "GuestCPU.hpp"
#include "GuestX64Backend.hpp"

#include <cstddef>
#include <cstdint>
#include <optional>
#include <vector>

namespace misaki {

// Minimal, bounded ELF64 ET_DYN dynamic table linking for self-authored test
// executables. This is NOT a Sony SELF/PRX or PS4 NID/module loader.
struct DynamicLinkSummary {
    std::uint64_t entry = 0;
    std::uint32_t mappedSegments = 0;
    std::uint32_t neededLibraries = 0;
    std::uint32_t importedSymbols = 0;
    std::uint32_t relativeRelocations = 0;
    std::uint64_t firstImportTarget = 0;
};

// Supports DT_NEEDED, DT_STRTAB/STRSZ, DT_SYMTAB/SYMENT, DT_RELA/RELASZ/
// RELAENT and R_X86_64_RELATIVE, GLOB_DAT and JUMP_SLOT. Both guest memory
// and permissions are left unchanged when parsing, loading or linking fails.
std::optional<DynamicLinkSummary> loadAndLinkDynamicELF(
    const std::uint8_t *elf, std::size_t length, std::uint64_t loadBias,
    const ModuleRegistry &modules, GuestMemory &memory);

std::vector<std::uint8_t> makeDynamicCPUFixture();

struct DynamicExecutionDiagnostic {
    X64ExecutionResult execution;
    DynamicLinkSummary linked;
    std::uint64_t relativeValue = 0;
    bool importReadOnly = false;
};

std::optional<DynamicExecutionDiagnostic> runDynamicCPUBackendDiagnostic();

} // namespace misaki
