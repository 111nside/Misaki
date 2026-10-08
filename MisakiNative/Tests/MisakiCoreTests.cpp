#include "MisakiCore.hpp"
#include "GuestCPU.hpp"
#include "MisakiCoreBridge.h"

#include <cstdint>
#include <cstdlib>
#include <iostream>
#include <limits>
#include <string>
#include <vector>

namespace {
int checked = 0;
void check(bool value, const char *message) {
    ++checked;
    if (!value) {
        std::cerr << "FAIL: " << message << '\n';
        std::exit(EXIT_FAILURE);
    }
}
void put(std::vector<std::uint8_t> &data, std::size_t start, std::uint64_t value,
         std::size_t count) {
    for (std::size_t i = 0; i < count; ++i)
        data[start + i] = std::uint8_t(value >> (8 * i));
}
}

int main() {
    using namespace misaki;
    auto data = makeDiagnosticELF();
    std::string error;
    auto info = inspectELF64(data.data(), data.size(), &error);
    check(bool(info), "valid test ELF");
    check(info && info->type == 3 && info->machine == 62, "ET_DYN x86-64");
    check(info && info->loadSegments == 1, "one PT_LOAD");
    check(info && info->entryPoint == 0x20, "ELF entry metadata");

    auto corrupted = data;
    corrupted[4] = 1;
    check(!inspectELF64(corrupted.data(), corrupted.size()), "reject ELF32");
    corrupted = data;
    corrupted[5] = 2;
    check(!inspectELF64(corrupted.data(), corrupted.size()), "reject big endian");
    corrupted = data;
    put(corrupted, 32, std::numeric_limits<std::uint64_t>::max(), 8);
    check(!inspectELF64(corrupted.data(), corrupted.size()), "reject invalid program header offset");
    corrupted = data;
    put(corrupted, 96, 0x999999, 8);
    check(!inspectELF64(corrupted.data(), corrupted.size()), "reject truncated segment");
    corrupted = data;
    put(corrupted, 104, 1, 8);
    check(!inspectELF64(corrupted.data(), corrupted.size()), "reject memsz smaller than filesz");
    check(!inspectELF64(data.data(), 42), "reject too short header");
    check(!inspectELF64(nullptr, 4096), "reject null file data");

    GuestMemory mem;
    check(mem.map(0x5000, 16, permission::read | permission::write), "map RW guest region");
    check(!mem.map(0x5008, 16, permission::read), "reject overlapping mappings");
    check(!mem.map(std::numeric_limits<std::uint64_t>::max() - 2, 16, permission::read), "reject overflowing mapping");
    check(mem.write64(0x5000, 0x1122334455667788ULL), "write guest pointer");
    check(mem.read64(0x5000) == 0x1122334455667788ULL, "read guest pointer");
    check(!mem.read64(0x500c), "reject out of bounds eight-byte read");
    check(!mem.isExecutable(0x5000), "non-executable mapping");
    check(mem.protect(0x5000, 16, permission::read), "protect region");
    check(!mem.write64(0x5000, 1), "reject write to protected region");
    check(mem.read64(0x5000) == 0x1122334455667788ULL, "failed write preserves bytes");
    check(!mem.protect(0x5000, 8, permission::write), "reject partial protection");

    ModuleRegistry registry;
    check(registry.registerModule({"libTest", {{"resolveMe", 0x9000}}}), "register guest module");
    check(!registry.registerModule({"libTest", {}}), "reject duplicate module");
    check(!registry.resolve("missing", "resolveMe"), "unknown guest library");
    check(!registry.resolve("libTest", "missing"), "unknown guest symbol");
    check(registry.resolve("libTest", "resolveMe") == 0x9000, "resolve symbol");
    check(!registry.bindImport(mem, 0x5000, "libTest", "resolveMe"), "reject bound RO import slot");
    check(mem.read64(0x5000) == 0x1122334455667788ULL, "RO import bind is atomic");
    check(mem.protect(0x5000, 16, permission::read | permission::write), "restore RW");
    check(registry.bindImport(mem, 0x5000, "libTest", "resolveMe"), "patch import pointer");
    check(mem.read64(0x5000) == 0x9000, "linked guest address matches export");
    check(!registry.bindImport(mem, 0x5008, "libTest", "unknown"), "missing import does not patch");
    check(mem.read64(0x5008) == 0, "missing import leaves slot unchanged");

    MisakiCoreReport report{};
    check(misaki_core_run_diagnostic(&report) == 0, "C ABI bridge diagnostic");
    check(report.abi_version == 1 && report.elf_type == 3, "C ABI report fields");
    check(report.load_segments == 1 && report.registered_modules == 1, "C ABI ELF and modules");
    check(report.resolved_imports == 1 && report.linked_guest_address == 0x6200, "C ABI import address");
    check(report.import_is_read_only == 1, "C ABI memory protection");
    check(misaki_core_run_diagnostic(nullptr) == -1, "C ABI rejects null report");
    std::uint32_t type = 0, segments = 0;
    check(misaki_core_inspect_elf(data.data(), data.size(), &type, &segments) == 0,
          "C ABI validates guest ELF");
    check(type == 3 && segments == 1, "C ABI parsed guest headers");
    check(misaki_core_inspect_elf(nullptr, 0, &type, &segments) == -2,
          "C ABI rejects invalid guest ELF");

    // Milestone 10: actually execute a self-authored ELF, across a linked library.
    const auto nativeRun = runGuestCPUDiagnostic();
    check(bool(nativeRun), "native guest ELF diagnostic prepared");
    check(nativeRun && nativeRun->cpu.stop == GuestStop::halted, "native guest halted");
    check(nativeRun && nativeRun->cpu.rax == 42, "native guest returned 42");
    check(nativeRun && nativeRun->cpu.instructions == 5, "CALL/MOV/ADD/RET/HLT = 5 instructions");
    check(nativeRun && nativeRun->cpu.stackRestored, "native guest stack restored");
    check(nativeRun && nativeRun->segments == 2 && nativeRun->imports == 1,
          "two guest ELF segments, one imported function");
    check(nativeRun && nativeRun->linkedAddress == 0x3000 && nativeRun->importReadOnly,
          "import address patched, then readonly");
    MisakiNativeCPUReport cpuReport{};
    check(misaki_core_run_cpu_diagnostic(&cpuReport) == 0, "native C API execution succeeds");
    check(cpuReport.abi_version == 1 && cpuReport.rax == 42 && cpuReport.instructions == 5,
          "native C API instruction report");
    check(cpuReport.halted && cpuReport.stack_restored && cpuReport.import_read_only,
          "native C API state and protection");
    check(cpuReport.loaded_segments == 2 && cpuReport.resolved_imports == 1 &&
          cpuReport.linked_address == 0x3000, "native C API linker report");
    check(misaki_core_run_cpu_diagnostic(nullptr) == -1, "native C API rejects null report");

    const auto execELF = makeGuestCPUFixture();
    GuestMemory exeMemory;
    const auto exeImage = loadGuestELF(execELF.data(), execELF.size(), 0, exeMemory);
    check(exeImage && exeImage->entry == 0x1000 && exeImage->segments == 2,
          "guest loader maps self-authored ELF segments");
    check(exeMemory.fetch8(0x1000) == 0x48, "guest ELF RX segment is executable");
    check(!exeMemory.writeBytes(0x1000, execELF.data(), 1), "guest RX segment is protected");
    check(!exeMemory.fetch8(0x1100), "guest GOT region is not executable");
    check(exeMemory.read64(0x1100) == 0, "guest GOT starts at zero");
    check(exeMemory.write64(0x1100, 0x3000), "guest GOT can be linked");
    check(exeMemory.protect(0x1100, 8, permission::read), "guest GOT protected after linking");
    check(!exeMemory.write64(0x1100, 0), "guest GOT stays immutable");

    auto broken = execELF;
    broken.resize(0x204);
    GuestMemory rollbackMemory;
    check(!loadGuestELF(broken.data(), broken.size(), 0, rollbackMemory),
          "guest loader rejects truncated program segment");
    check(!rollbackMemory.read8(0x1000), "failed load rolls back mapped code");
    broken = execELF;
    put(broken, 136, 0x1004, 8);
    check(!loadGuestELF(broken.data(), broken.size(), 0, rollbackMemory),
          "guest loader rejects overlapping PT_LOAD segments");
    check(!rollbackMemory.read8(0x1000), "overlap does not partially map");
    broken = execELF;
    put(broken, 24, 0x5000, 8);
    check(!loadGuestELF(broken.data(), broken.size(), 0, rollbackMemory),
          "guest loader rejects entry point outside executable segment");

    broken = execELF;
    put(broken, 16, 3, 2); // ET_DYN
    GuestMemory dynMemory;
    auto dynImage = loadGuestELF(broken.data(), broken.size(), 0x4000, dynMemory);
    check(dynImage && dynImage->entry == 0x5000 && dynImage->segments == 2,
          "position-independent ELF load bias maps executable entry");
    check(dynMemory.fetch8(0x5000) == 0x48 && dynMemory.read64(0x5100) == 0,
          "ET_DYN segments were relocated by guest load bias");

    GuestMemory faultMemory;
    check(faultMemory.map(0x1000, 1, permission::read | permission::write),
          "map single-byte invalid-instruction test");
    const std::uint8_t invalid = 0xCC;
    check(faultMemory.writeBytes(0x1000, &invalid, 1), "write invalid guest instruction");
    check(faultMemory.protect(0x1000, 1, permission::read | permission::execute),
          "protect invalid guest instruction");
    GuestCPU invalidCPU(faultMemory, 0x1000);
    check(invalidCPU.run().stop == GuestStop::invalidOpcode, "invalid opcode traps safely");
    GuestCPU nonExecutableCPU(faultMemory, 0x5000);
    check(nonExecutableCPU.run().stop == GuestStop::memoryFault,
          "unmapped guest execution traps safely");

    GuestMemory loopMemory;
    check(loopMemory.map(0x1000, 2, permission::read | permission::write),
          "map conditional loop");
    const std::uint8_t loop[] = {0x75, 0xFE}; // ZF starts false; JNE -2
    check(loopMemory.writeBytes(0x1000, loop, sizeof(loop)), "write branch loop");
    check(loopMemory.protect(0x1000, 2, permission::read | permission::execute),
          "protect conditional loop");
    GuestCPU looping(loopMemory, 0x1000);
    const auto capped = looping.run(7);
    check(capped.stop == GuestStop::stepLimit && capped.instructions == 7,
          "infinite guest branch bounded by instruction budget");

    GuestMemory callMemory;
    const std::uint8_t callProgram[] = {
        0xE8, 0x01, 0, 0, 0, 0xF4, // call +1; hlt
        0x48, 0xB8, 42, 0, 0, 0, 0, 0, 0, 0, 0xC3 // mov rax,42; ret
    };
    check(callMemory.map(0x2000, sizeof(callProgram), permission::read | permission::write),
          "map direct CALL/RET fixture");
    check(callMemory.writeBytes(0x2000, callProgram, sizeof(callProgram)),
          "write direct CALL/RET fixture");
    check(callMemory.protect(0x2000, sizeof(callProgram),
                             permission::read | permission::execute), "protect direct CALL/RET fixture");
    GuestCPU directCalls(callMemory, 0x2000);
    const auto callRun = directCalls.run(12);
    check(callRun.stop == GuestStop::halted && callRun.rax == 42 &&
          callRun.instructions == 4 && callRun.stackRestored,
          "direct guest CALL -> MOV -> RET -> HLT works");

    GuestMemory protectionMem;
    check(protectionMem.map(0x2000, 4, permission::read | permission::write),
          "map writable source for atomic write test");
    check(protectionMem.map(0x2004, 4, permission::read),
          "map read-only tail for atomic write test");
    const std::uint8_t input8[] = {1, 2, 3, 4, 5, 6, 7, 8};
    check(!protectionMem.writeBytes(0x2000, input8, 8),
          "multi-region write is rejected atomically");
    check(protectionMem.read8(0x2000) == 0, "atomic write left source unmodified");

    std::cout << "PASS: " << checked << " native C++ assertions\n";
}
