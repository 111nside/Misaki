#include "MisakiCore.hpp"
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

    std::cout << "PASS: " << checked << " native C++ assertions\n";
}
