#include "GuestSystemLibraries.hpp"
#include "MisakiCoreBridge.h"

#include <cstdlib>
#include <iostream>
#include <string>
#include <vector>

namespace {
int count = 0;
void check(bool yes, const char *message) {
    ++count;
    if (!yes) {
        std::cerr << "FAIL: " << message << '\n';
        std::exit(EXIT_FAILURE);
    }
}
}

int main() {
    using namespace misaki;
    const auto runtime = runGuestSystemLibraryDiagnostic();
    check(bool(runtime), "integrated guest ELF setup and dynamic linking");
    if (!runtime) return EXIT_FAILURE;
    const auto &r = *runtime;
    check(r.linked.entry == 0x5000, "position independent ET_DYN entry");
    check(r.linked.mappedSegments == 2, "ELF code and data segments mapped");
    check(r.linked.neededLibraries == 1, "real DT_NEEDED parsed");
    check(r.linked.importedSymbols == 1, "symbol binding resolved");
    check(r.linked.relativeRelocations == 1, "relative relocation resolved");
    check(r.linked.firstImportTarget == 0x9000, "import points to guest library");
    check(r.importAddress == 0x9000, "GOT slot patched");
    check(r.importReadOnly, "GOT mapping is read only after binding");
    check(r.libraryReadOnly, "guest library is executable and not writable");
    check(r.execution.stop == X64Stop::halted, "guest execution halted cleanly");
    check(r.execution.rax() == 42, "guest returned 42 from page-size arithmetic");
    check(r.execution.stackRestored(), "library RET restored guest stack");
    check(r.execution.state.rip == 0x5012, "guest returned to final HLT");
    check(r.execution.state.instructions > 15, "guest actually executed multi-step library");
    check(r.execution.state.instructions < 70, "guest execution stayed bounded");
    check(r.processID == 1001, "private process ID for service dispatcher");
    check(r.opens == 1, "guest file opened through service dispatcher");
    check(r.reads == 1, "guest file read through service dispatcher");
    check(r.closes == 1, "guest descriptor closed through service dispatcher");
    check(r.writes == 1, "guest buffer sent through toy write service");
    check(r.yields == 0, "guest returns without yielding");
    check(r.output == "Hello", "guest library output came from virtual file");

    const auto code = makeGuestSystemLibraryCode();
    check(!code.empty(), "library contains executable guest instructions");
    check(code.size() <= 512, "library stays within explicit safety bound");
    check(code.back() == 0xC3, "guest library ends with a real x86 RET");
    check(code[0] == 0x48 && code[1] == 0xB8, "library begins with MOV RAX,imm64");

    GuestVirtualFileSystem files;
    check(files.addFile("/system/message.txt", {'H','e','l','l','o'}), "guest fixture path canonical");
    check(!files.addFile("/system/message.txt", {'X'}), "duplicate library file rejected");
    check(!files.addFile("/system/../../private", {'X'}), "traversal rejected");
    check(!files.addFile("/system//private", {'X'}), "empty path component rejected");
    check(!files.addFile("relative.txt", {'X'}), "relative path rejected");
    check(files.findFile("/system/message.txt") != nullptr, "sandbox file visible");
    check(files.findFile("/host/path") == nullptr, "host filesystem not exposed");

    const auto elf = makeDynamicCPUFixture();
    GuestMemory memory;
    ModuleRegistry missing;
    check(!loadAndLinkDynamicELF(elf.data(), elf.size(), 0x4000, missing, memory),
          "unresolved system import rejects launch");
    check(!memory.read8(0x5000), "failed module load rolls back guest mappings");

    ModuleRegistry wrong;
    check(wrong.registerModule({"libMisakiDynamic", {{"anotherFunction", 0x9000}}}),
          "wrong export set accepted for negative test");
    check(!loadAndLinkDynamicELF(elf.data(), elf.size(), 0x4000, wrong, memory),
          "unknown library symbol rejects link");
    check(!memory.read8(0x5100), "failed link never publishes GOT memory");

    auto malformed = elf;
    malformed[4] = 1; // invalid class, not ELF64
    ModuleRegistry good;
    check(good.registerModule({"libMisakiDynamic", {{"increment2", 0x9000}}}),
          "valid registry setup");
    check(!loadAndLinkDynamicELF(malformed.data(), malformed.size(), 0x4000, good, memory),
          "malformed executable is rejected");
    check(!memory.read8(0x5000), "malformed executable leaves memory unchanged");

    // Repeat the full run to catch hidden state leaks between process sessions.
    const auto second = runGuestSystemLibraryDiagnostic();
    check(bool(second), "fresh process runs independently");
    check(second && second->output == "Hello" && second->opens == 1,
          "fresh session has its own VFS/FD counters");
    check(second && second->execution.rax() == r.execution.rax(),
          "deterministic guest result across sessions");

    MisakiSystemLibraryReport bridge{};
    check(misaki_core_run_system_library_diagnostic(&bridge) == 0,
          "system library diagnostic callable from Swift bridge");
    check(bridge.abi_version == 1 && bridge.rax == 42 && bridge.instructions == 26,
          "C ABI exports CPU result and instruction count");
    check(bridge.imported_symbols == 1 && bridge.relative_relocations == 1 &&
          bridge.loaded_segments == 2, "C ABI exports native ET_DYN module state");
    check(bridge.import_read_only && bridge.library_read_only && bridge.stack_restored,
          "C ABI exports memory and stack invariants");
    check(bridge.opens == 1 && bridge.reads == 1 && bridge.closes == 1 &&
          bridge.writes == 1 && bridge.output_matches,
          "C ABI exports virtual-filesystem service counters");
    check(misaki_core_run_system_library_diagnostic(nullptr) == -1,
          "C ABI safely rejects null report");

    std::cout << "PASS: " << count << " guest system-library assertions\n";
}
