#include "GuestDynamicExecution.hpp"
#include "MisakiCoreBridge.h"

#include <cstdint>
#include <cstdlib>
#include <iostream>
#include <string>
#include <vector>

namespace {
unsigned assertions = 0;
void check(bool condition, const char *why) {
    ++assertions;
    if (!condition) {
        std::cerr << "FAIL (assertion " << assertions << "): " << why << '\n';
        std::exit(1);
    }
}
void put(std::vector<std::uint8_t> &buffer, std::size_t offset,
         std::uint64_t value, unsigned width) {
    for (unsigned i = 0; i < width; ++i)
        buffer[offset + i] = static_cast<std::uint8_t>(value >> (8 * i));
}
struct Setup {
    misaki::GuestMemory memory;
    misaki::ModuleRegistry modules;
    explicit Setup(bool goodLibrary = true) {
        constexpr std::uint8_t code[] = {0x48, 0x83, 0xc0, 0x02, 0xc3};
        check(memory.map(0x9000, sizeof(code), misaki::permission::read | misaki::permission::write), "map library");
        check(memory.writeBytes(0x9000, code, sizeof(code)), "write library");
        check(memory.protect(0x9000, sizeof(code), misaki::permission::read | misaki::permission::execute), "protect library");
        check(modules.registerModule({"libMisakiDynamic", {{goodLibrary ? "increment2" : "wrongName", 0x9000}}}), "register library");
    }
    std::optional<misaki::DynamicLinkSummary> link(std::vector<std::uint8_t> &elf,
                                                    std::uint64_t bias = 0x4000) {
        return misaki::loadAndLinkDynamicELF(elf.data(), elf.size(), bias, modules, memory);
    }
};
}

int main() {
    using namespace misaki;
    auto fixture = makeDynamicCPUFixture();
    check(fixture.size() == 0x440, "synthetic fixture size");
    auto inspected = inspectELF64(fixture.data(), fixture.size());
    check(bool(inspected), "ELF64 header is valid");
    check(inspected && inspected->type == 3 && inspected->hasDynamicSegment, "ET_DYN with PT_DYNAMIC");
    check(inspected && inspected->loadSegments == 2, "two loadable ELF segments");
    Setup healthy;
    auto linked = healthy.link(fixture);
    check(bool(linked), "dynamic file loaded and linked");
    check(linked && linked->entry == 0x5000 && linked->mappedSegments == 2, "load bias and entry");
    check(linked && linked->neededLibraries == 1 && linked->importedSymbols == 1, "DT_NEEDED and JUMP_SLOT");
    check(linked && linked->relativeRelocations == 1, "RELATIVE relocation count");
    check(linked && linked->firstImportTarget == 0x9000, "dynamic symbol resolved");
    check(healthy.memory.read64(0x5100) == 0x9000, "GOT was patched");
    check(healthy.memory.read64(0x5108) == 0x5234, "relative relocation adds load bias");
    check(healthy.memory.fetch8(0x5000) == 0x48, "RX code was mapped executable");
    check(!healthy.memory.write64(0x5000, 0), "RX code refuses writes");
    check(!healthy.memory.fetch8(0x5100), "RW guest data refuses execution");
    check(healthy.memory.protect(0x5100, 0x240, permission::read), "GOT protect after relocation");
    check(!healthy.memory.write64(0x5100, 0), "GOT becomes read only");
    PortableX64Backend cpu(healthy.memory, linked->entry);
    auto result = cpu.run(30);
    check(result.stop == X64Stop::halted, "dynamic code halts normally");
    check(result.rax() == 42, "dynamic executable calls imported function");
    check(result.state.instructions == 5, "MOV/CALL/ADD/RET/HLT instruction count");
    check(result.stackRestored(), "stack returns to initial state");
    check(healthy.memory.read64(0x5108) == 0x5234, "protected relative relocation intact");

    // A second load bias makes all guest virtual addresses change but leaves
    // the separately mapped library function at its registered guest address.
    Setup alternate;
    auto relink = alternate.link(fixture, 0xa000);
    check(relink && relink->entry == 0xb000, "ET_DYN relocated to alternate base");
    check(alternate.memory.read64(0xb100) == 0x9000, "import binding is base independent");
    check(alternate.memory.read64(0xb108) == 0xb234, "RELATIVE uses alternate load bias");
    PortableX64Backend alternativeCPU(alternate.memory, 0xb000);
    auto alternateResult = alternativeCPU.run(30);
    check(alternateResult.stop == X64Stop::halted && alternateResult.rax() == 42,
          "dynamic executable runs from second base");

    // A GLOB_DAT import uses the same symbol resolution contract as JUMP_SLOT.
    {
        auto globDat = fixture;
        put(globDat, 0x420, (std::uint64_t(1) << 32) | 6, 8);
        Setup state;
        auto loaded = state.link(globDat);
        check(loaded && loaded->importedSymbols == 1, "GLOB_DAT import resolves");
        check(state.memory.read64(0x5100) == 0x9000, "GLOB_DAT patches guest pointer");
        PortableX64Backend guest(state.memory, 0x5000);
        auto output = guest.run(30);
        check(output.stop == X64Stop::halted && output.rax() == 42,
              "GLOB_DAT-linked function executes");
    }

    auto checkRejected = [&](std::vector<std::uint8_t> elf, const char *description,
                             bool wrongSymbol = false) {
        Setup state(!wrongSymbol);
        check(state.memory.map(0x7800, 8, permission::read | permission::write), "map sentinel");
        check(state.memory.write64(0x7800, 0x123456789abcdef0ULL), "initialize sentinel");
        check(!state.link(elf), description);
        check(state.memory.read64(0x7800) == 0x123456789abcdef0ULL,
              "failed load keeps existing memory intact");
        check(!state.memory.read8(0x5100), "failed load rolls back GOT");
        check(!state.memory.fetch8(0x5000), "failed load rolls back code");
    };
    {
        auto file = fixture;
        put(file, 16, 2, 2); // ET_EXEC
        checkRejected(file, "ET_EXEC rejected by dynamic module loader");
    }
    {
        auto file = fixture;
        put(file, 192, 0x3000, 8); // PT_DYNAMIC points beyond file-backed load
        checkRejected(file, "PT_DYNAMIC must belong to file-backed PT_LOAD");
    }
    {
        auto file = fixture;
        put(file, 176, 0, 4);
        checkRejected(file, "missing PT_DYNAMIC rejected");
    }
    {
        auto file = fixture;
        put(file, 208, 3000, 8);
        checkRejected(file, "truncated PT_DYNAMIC rejected");
    }
    {
        auto file = fixture;
        put(file, 0x240 + 8 * 16, 5, 8); // no DT_NULL
        checkRejected(file, "missing DT_NULL terminator rejected");
    }
    {
        auto file = fixture;
        put(file, 0x240, 5, 8); // duplicate DT_STRTAB and missing DT_NEEDED
        checkRejected(file, "duplicate dynamic tag rejected");
    }
    {
        auto file = fixture;
        put(file, 0x240 + 16 + 8, 0xffffffffffffffffULL, 8);
        checkRejected(file, "out-of-file string table rejected");
    }
    {
        auto file = fixture;
        put(file, 0x240 + 3 * 16 + 8, 8192, 8);
        checkRejected(file, "oversized string table rejected");
    }
    {
        auto file = fixture;
        put(file, 0x240 + 6 * 16 + 8, 47, 8);
        checkRejected(file, "invalid RELA size rejected");
    }
    {
        auto file = fixture;
        put(file, 0x240 + 7 * 16 + 8, 16, 8);
        checkRejected(file, "invalid RELA entry width rejected");
    }
    {
        auto file = fixture;
        put(file, 0x420, (std::uint64_t(1) << 32) | 9, 8);
        checkRejected(file, "unsupported relocation type rejected");
    }
    {
        auto file = fixture;
        put(file, 0x420, (std::uint64_t(300) << 32) | 7, 8);
        checkRejected(file, "unbounded symbol index rejected");
    }
    {
        auto file = fixture;
        put(file, 0x300 + 24, 4095, 4);
        checkRejected(file, "symbol outside string table rejected");
    }
    {
        auto file = fixture;
        checkRejected(file, "unresolved symbol rejected", true);
    }
    {
        auto file = fixture;
        put(file, 0x418, 0x1000, 8); // relocate in RX code
        checkRejected(file, "write into RX segment rejected");
    }
    {
        auto file = fixture;
        put(file, 0x400, 0x1100, 8); // duplicate relocation destination
        checkRejected(file, "duplicate relocation target rejected");
    }
    {
        auto file = fixture;
        put(file, 0x410, static_cast<std::uint64_t>(-0x7000LL), 8);
        checkRejected(file, "negative relative relocation underflows");
    }
    {
        auto file = fixture;
        put(file, 0x420, (std::uint64_t(1) << 32) | 7, 8);
        put(file, 0x428, 1, 8);
        checkRejected(file, "unsupported symbolic relocation addend rejected");
    }
    {
        auto file = fixture;
        put(file, 0x300 + 24 + 6, 1, 2); // not undefined
        checkRejected(file, "non-undefined imported symbol rejected");
    }
    {
        auto file = fixture;
        put(file, 0x248, 4095, 8); // DT_NEEDED beyond DT_STRSZ
        checkRejected(file, "invalid library string offset rejected");
    }
    {
        auto file = fixture;
        put(file, 24, 0x2000, 8); // entry outside executable PT_LOAD
        checkRejected(file, "non-executable entry point rejected");
    }
    {
        auto file = fixture;
        file.resize(0x410);
        checkRejected(file, "truncated relocation table rejected");
    }
    {
        auto file = fixture;
        put(file, 0x240 + 5 * 16 + 8, 0x1400, 8);
        checkRejected(file, "relocations outside PT_LOAD rejected");
    }

    const auto integration = runDynamicCPUBackendDiagnostic();
    check(bool(integration), "end-to-end ET_DYN diagnostic exists");
    check(integration && integration->execution.stop == X64Stop::halted,
          "end-to-end ET_DYN halts");
    check(integration && integration->execution.rax() == 42,
          "end-to-end ET_DYN returns 42");
    check(integration && integration->execution.state.instructions == 5,
          "end-to-end instruction count");
    check(integration && integration->execution.stackRestored(),
          "end-to-end stack restored");
    check(integration && integration->linked.importedSymbols == 1 &&
          integration->linked.relativeRelocations == 1, "end-to-end relocations");
    check(integration && integration->relativeValue == 0x5234,
          "end-to-end relative pointer value");
    check(integration && integration->importReadOnly, "end-to-end import protected");

    MisakiDynamicCPUReport bridge{};
    check(misaki_core_run_dynamic_cpu_diagnostic(&bridge) == 0, "C ABI dynamic execution succeeds");
    check(bridge.abi_version == 1 && bridge.rax == 42, "C ABI dynamic CPU register");
    check(bridge.entry == 0x5000 && bridge.linked_address == 0x9000,
          "C ABI relocated entry and import address");
    check(bridge.relative_value == 0x5234 && bridge.relative_relocations == 1,
          "C ABI relative relocation");
    check(bridge.instructions == 5 && bridge.imports == 1 && bridge.halted &&
          bridge.stack_restored && bridge.import_read_only, "C ABI execution status");
    check(misaki_core_run_dynamic_cpu_diagnostic(nullptr) == -1, "C ABI null rejected");

    std::cout << "PASS: " << assertions << " dynamic module assertions\n";
}
