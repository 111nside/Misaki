#include "GuestLibraryCatalog.hpp"
#include "MisakiCoreBridge.h"

#include <cstdint>
#include <cstdlib>
#include <iostream>
#include <limits>
#include <string>
#include <vector>

namespace {
unsigned assertions = 0;
void check(bool condition, const char *name) {
    ++assertions;
    if (!condition) { std::cerr << "FAIL: " << name << '\n'; std::exit(1); }
}
void put(std::vector<std::uint8_t> &bytes, std::size_t at,
         std::uint64_t value, unsigned width) {
    for (unsigned i=0; i<width; ++i) bytes[at+i] = std::uint8_t(value >> (8*i));
}
}
int main() {
    using namespace misaki;
    GuestLibraryCatalog catalog;
    check(catalog.versionCount() == 0 && catalog.libraryCount() == 0, "empty catalog");
    check(!catalog.registerLibrary({"../bad", {1,0}, {{"ok",0x9000}}}), "reject traversal names");
    check(!catalog.registerLibrary({"lib A", {1,0}, {{"ok",0x9000}}}), "reject spaces");
    check(!catalog.registerLibrary({"libGood", {0,1}, {{"ok",0x9000}}}), "reject major zero");
    check(!catalog.registerLibrary({"libGood", {1,0}, {}}), "reject no exports");
    check(!catalog.registerLibrary({"libGood", {1,0}, {{"ok",0}}}), "reject null address");
    check(!catalog.registerLibrary({"libGood", {1,0}, {{"/host",0x9000}}}), "reject path-like symbols");
    check(catalog.registerLibrary({"libGood", {1,0}, {{"fn",0x9000}}}), "register version 1.0");
    check(catalog.registerLibrary({"libGood", {1,4}, {{"fn",0x9010}}}), "register version 1.4");
    check(catalog.registerLibrary({"libGood", {2,1}, {{"fn",0xA000}}}), "register version 2.1");
    check(!catalog.registerLibrary({"libGood", {1,4}, {{"fn",0x1234}}}), "reject identical version");
    check(catalog.libraryCount() == 1 && catalog.versionCount() == 3, "library/version counts");
    check(catalog.resolve("libGood", "fn") == 0xA000, "default newest version");
    check(catalog.resolve("libGood", "fn", 1, 3) == 0x9010, "compatible pinned major");
    check(catalog.resolve("libGood", "fn", 1, 5) == std::nullopt, "reject absent minor");
    check(catalog.resolve("libGood", "fn", 3) == std::nullopt, "reject absent major");
    check(catalog.resolve("libGood", "missing") == std::nullopt, "reject absent symbol");
    check(catalog.resolve("missing", "fn") == std::nullopt, "reject absent library");
    check(catalog.selectedVersion("libGood") == CatalogVersion{2,1}, "report selected version");
    check(!catalog.selectedVersion("missing"), "unknown version absent");
    check(catalog.registerLibrary({"libSecond", {1,0}, {{"fn",0xB000}}}), "second library");
    check(catalog.libraryCount() == 2, "two unique library names");
    const auto snapshot = catalog.snapshot();
    check(bool(snapshot), "catalog snapshot");
    check(snapshot && snapshot->count() == 2, "snapshot has two namespaces");
    check(snapshot && snapshot->resolve("libGood", "fn") == 0xA000, "snapshot selects latest");
    check(snapshot && snapshot->resolve("libSecond", "fn") == 0xB000, "snapshot second library");

    const auto diagnostic = runCatalogDynamicDiagnostic();
    check(bool(diagnostic), "integrated catalog and relocation fixture");
    check(diagnostic && diagnostic->execution.stop == X64Stop::halted, "backend reaches HLT");
    check(diagnostic && diagnostic->execution.rax() == 42, "returns 42");
    check(diagnostic && diagnostic->execution.state.instructions == 5, "five executed steps");
    check(diagnostic && diagnostic->execution.stackRestored(), "stack restored");
    check(diagnostic && diagnostic->linked.importedSymbols == 6, "six symbol relocations");
    check(diagnostic && diagnostic->linked.relativeRelocations == 1, "one relative relocation");
    check(diagnostic && diagnostic->linked.absoluteRelocations == 3, "three absolute relocations");
    check(diagnostic && diagnostic->linked.pcRelativeRelocations == 2, "two relative32 relocations");
    check(diagnostic && diagnostic->catalogVersions == 2, "two versions in execution catalog");
    check(diagnostic && diagnostic->absolute64 == 0x8FF8, "ABS64 S+A");
    check(diagnostic && diagnostic->unsigned32 == 0x9004, "ABS32 S+A");
    check(diagnostic && diagnostic->signed32 == 0x8FFC, "ABS32S S+A");
    check(diagnostic && diagnostic->relative32 == 0x3ECC, "PC32 S+A-P");
    check(diagnostic && diagnostic->plt32 == 0x3EC4, "PLT32 S+A-P");
    check(diagnostic && diagnostic->importReadOnly, "GOT read-only after linking");

    auto elf = makeCatalogDynamicFixture();
    GuestLibraryCatalog current;
    check(current.registerLibrary({"libMisakiDynamic", {1,2}, {{"increment2",0x9000}}}), "set up catalog");
    {
        GuestMemory memory;
        auto linked = loadAndLinkCatalogELF(elf.data(), elf.size(), 0x4000, current, memory);
        check(bool(linked), "extended relocations load and link");
        check(memory.read64(0x5110) == 0x8FF8, "ABS64 memory result");
        check(memory.read64(0x5120) == 0x9004, "ABS32 memory result");
        check(memory.read64(0x5128) == 0x8FFC, "ABS32S memory result");
        check(memory.read64(0x5130) == 0x3ECC, "PC32 memory result");
        check(memory.read64(0x5138) == 0x3EC4, "PLT32 memory result");
    }
    {
        auto bad = elf;
        put(bad, 0x430 + 8, (std::uint64_t(1)<<32) | 99, 8);
        GuestMemory mem;
        check(!loadAndLinkCatalogELF(bad.data(), bad.size(), 0x4000, current, mem),
              "unknown relocation rejected");
        check(!mem.read8(0x5100), "unknown relocation is transactional");
    }
    {
        auto bad = elf;
        put(bad, 0x430 + 8, (std::uint64_t(1)<<32) | 10, 8); // ABS32
        put(bad, 0x430 + 16, 0x1'0000'0000ULL, 8);
        GuestMemory mem;
        check(!loadAndLinkCatalogELF(bad.data(), bad.size(), 0x4000, current, mem),
              "ABS32 overflow rejected");
        check(!mem.read8(0x5100), "overflow rolls back image");
    }
    {
        auto bad = elf;
        const std::size_t at = 0x430 + 2*24;
        put(bad, at + 16, 0x7FFF'FFFFULL, 8); // S+A would exceed INT32_MAX
        GuestMemory mem;
        check(!loadAndLinkCatalogELF(bad.data(), bad.size(), 0x4000, current, mem),
              "ABS32S overflow rejected");
    }
    {
        auto bad = elf;
        put(bad, 0x430 + 3*24 + 16, 0x1'0000'0000ULL, 8);
        GuestMemory mem;
        check(!loadAndLinkCatalogELF(bad.data(), bad.size(), 0x4000, current, mem),
              "PC32 displacement overflow rejected");
    }
    {
        auto bad = elf;
        put(bad, 0x430 + 4*24, 0x1122, 8); // overlaps earlier ABS32
        GuestMemory mem;
        check(!loadAndLinkCatalogELF(bad.data(), bad.size(), 0x4000, current, mem),
              "overlapping relocation byte ranges rejected");
    }
    {
        auto bad = elf;
        put(bad, 0x430, 0x1000, 8); // RX code cannot be patched
        GuestMemory mem;
        check(!loadAndLinkCatalogELF(bad.data(), bad.size(), 0x4000, current, mem),
              "relocation into code RX rejected");
    }
    {
        GuestLibraryCatalog absent;
        GuestMemory mem;
        check(!loadAndLinkCatalogELF(elf.data(), elf.size(), 0x4000, absent, mem),
              "missing library import rejected");
    }
    {
        auto bad = elf;
        put(bad, 0x430 + 8, (std::uint64_t(2)<<32) | 1, 8);
        GuestMemory mem;
        check(!loadAndLinkCatalogELF(bad.data(), bad.size(), 0x4000, current, mem),
              "invalid symbol index rejected");
    }
    {
        // R_X86_64_32S accepts a negative sign-extended value.
        auto single = elf;
        put(single, 0x2A8, 24, 8); // one RELA entry
        put(single, 0x400, 0x1128, 8);
        put(single, 0x408, (std::uint64_t(1)<<32) | 11, 8);
        put(single, 0x410, static_cast<std::uint64_t>(-4LL), 8);
        GuestLibraryCatalog negative;
        check(negative.registerLibrary({"libMisakiDynamic", {1,0},
              {{"increment2",0xffff'ffff'ffff'ff00ULL}}}), "negative address catalog");
        GuestMemory mem;
        const auto result = loadAndLinkCatalogELF(single.data(), single.size(),
                                                   0x4000, negative, mem);
        check(bool(result), "ABS32S negative sign-extension accepted");
        check(mem.read64(0x5128) == 0xffff'fefcULL, "ABS32S negative bytes exact");
    }
    {
        // R_X86_64_PC32 can encode a negative displacement without host overflow.
        auto single = elf;
        put(single, 0x2A8, 24, 8);
        put(single, 0x400, 0x1130, 8);
        put(single, 0x408, (std::uint64_t(1)<<32) | 2, 8);
        put(single, 0x410, static_cast<std::uint64_t>(-4LL), 8);
        GuestLibraryCatalog negative;
        check(negative.registerLibrary({"libMisakiDynamic", {1,0},
              {{"increment2",0x3000}}}), "negative PC32 catalog");
        GuestMemory mem;
        const auto result = loadAndLinkCatalogELF(single.data(), single.size(),
                                                   0x4000, negative, mem);
        check(bool(result), "negative PC32 displacement accepted");
        check(mem.read64(0x5130) == 0xffff'deccULL, "negative PC32 bytes exact");
    }
    MisakiLibraryCatalogReport bridge{};
    check(misaki_core_run_catalog_diagnostic(&bridge) == 0, "C bridge diagnostic passes");
    check(bridge.abi_version == 1 && bridge.rax == 42 && bridge.instructions == 5,
          "C bridge CPU state");
    check(bridge.imported_symbols == 6 && bridge.absolute_relocations == 3 &&
          bridge.pc_relative_relocations == 2, "C bridge relocation summary");
    check(bridge.catalog_versions == 2 && bridge.stack_restored && bridge.import_read_only,
          "C bridge version, stack, permissions");
    check(misaki_core_run_catalog_diagnostic(nullptr) == -1, "C bridge null rejected");
    std::cout << "PASS: " << assertions << " library catalog and relocation assertions\n";
}
