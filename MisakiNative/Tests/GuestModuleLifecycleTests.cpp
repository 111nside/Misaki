#include "GuestModuleLifecycle.hpp"
#include "MisakiModuleLifecycleBridge.h"

#include <cstdlib>
#include <iostream>
#include <limits>
#include <string>
#include <vector>

namespace {
int checked = 0;
void check(bool valid, const char *message) {
    ++checked;
    if (!valid) { std::cerr << "FAIL: " << message << '\n'; std::exit(EXIT_FAILURE); }
}
misaki::GuestModuleDefinition mod(std::string name, std::uint64_t addr,
                                  std::vector<std::string> deps = {}) {
    return {name, addr, {0x90, 0xC3}, deps, {{"entry", addr}}};
}
}

int main() {
    using namespace misaki;
    {
        GuestModuleLifecycle manager;
        check(manager.define(mod("foundation", 0x4000)), "register foundation");
        check(manager.define(mod("utility", 0x5000, {"foundation"})), "register utility");
        check(manager.define(mod("application", 0x6000, {"utility"})), "register app");
        check(manager.definitionCount() == 3 && manager.loadedCount() == 0, "definitions don't auto-load");
        check(manager.acquire("application"), "load app dependency chain");
        check(manager.loadedCount() == 3, "three loaded modules");
        check(manager.loaded("foundation") && manager.loaded("utility"), "dependencies active");
        check(manager.references("foundation") == 1, "transitive dependency retained");
        check(manager.opens("application") == 1 && manager.references("application") == 1, "app root references");
        check(manager.initializations() == 3 && manager.finalizations() == 0, "initialization count");
        check(manager.events().size() == 3 && manager.events()[0].name == "foundation" &&
              manager.events()[1].name == "utility" && manager.events()[2].name == "application", "dependencies initialized first");
        check(manager.memory().fetch8(0x4000) == 0x90, "mapped foundation executable");
        auto guestCopy = manager.memory(); check(!guestCopy.write64(0x4000, 1), "module RX memory not writable");
        auto registry = manager.activeRegistry();
        check(registry && registry->resolve("foundation", "entry") == 0x4000, "export discovered");
        check(manager.acquire("application") && manager.opens("application") == 2, "second open increments root ref");
        check(manager.references("utility") == 1 && manager.initializations() == 3, "repeat open doesn't reinit dependency");
        check(manager.release("application") && manager.loadedCount() == 3, "first close leaves chain mapped");
        check(manager.opens("application") == 1 && manager.finalizations() == 0, "first close keeps module initialized");
        check(manager.release("application") && manager.loadedCount() == 0, "final close unloads whole chain");
        check(!manager.memory().fetch8(0x4000), "module memory unmapped after release");
        check(manager.finalizations() == 3 && manager.events().size() == 6, "finalization count");
        check(manager.events()[3].name == "application" && manager.events()[4].name == "utility" &&
              manager.events()[5].name == "foundation", "finalize parents before dependencies");
        check(!manager.release("application"), "cannot release closed module");
        check(manager.acquire("application") && manager.initializations() == 6, "reload initializes again");
        check(manager.release("application") && manager.finalizations() == 6, "reload finalizes again");
    }
    {
        GuestModuleLifecycle m;
        check(m.define(mod("shared", 0x2000)), "define shared");
        check(m.define(mod("left", 0x3000, {"shared"})), "define left");
        check(m.define(mod("right", 0x4000, {"shared"})), "define right");
        check(m.acquire("left") && m.acquire("right"), "load two users of common dependency");
        check(m.references("shared") == 2 && m.initializations() == 3, "shared dependency has two references, initialized once");
        check(m.release("left") && m.loaded("shared") && m.references("shared") == 1, "shared remains after first user closes");
        check(m.loaded("right") && !m.loaded("left"), "unneeded parent unloaded");
        check(m.release("right") && m.loadedCount() == 0, "last user releases shared module");
        check(m.initializations() == m.finalizations(), "balanced init/finalize");
    }
    {
        GuestModuleLifecycle m;
        check(m.define(mod("one", 0x1000, {"missing"})), "forward dependency permitted");
        check(!m.acquire("one") && m.loadedCount() == 0, "missing dependency rejected atomically");
        check(m.define(mod("missing", 0x2000, {"one"})), "cycle definitions accepted");
        check(!m.acquire("one") && m.events().empty(), "cycle cannot be loaded");
        check(m.define(mod("safe", 0x3000)), "unrelated valid definition");
        check(m.acquire("safe") && m.loadedCount() == 1, "cycle failure didn't poison manager");
        check(!m.acquire("one") && m.loadedCount() == 1 && m.loaded("safe"), "failed acquire leaves working modules intact");
        check(m.release("safe"), "release safe module");
    }
    {
        GuestModuleLifecycle m;
        check(!m.define(mod("../secret", 0x1000)), "reject traversal in guest name");
        check(!m.define(mod("bad name", 0x1000)), "reject whitespace in guest name");
        check(!m.define(mod("", 0x1000)), "reject empty guest name");
        check(!m.define(mod("bad", std::numeric_limits<std::uint64_t>::max())), "reject overflowing image bounds");
        check(!m.define({"empty", 0x1000, {}, {}, {}}), "reject empty guest module image");
        check(!m.define({"invalid_export", 0x1000, {0xC3}, {}, {{"entry", 0x1001}}}), "reject export outside image");
        check(!m.define({"duplicate_deps", 0x1000, {0xC3}, {"same", "same"}, {}}), "reject duplicate deps");
        check(m.define(mod("valid", 0x1000)), "accept valid module");
        check(!m.define(mod("valid", 0x2000)), "reject duplicate module definition");
        check(!m.acquire("missing") && !m.release("valid"), "reject invalid acquire/release");
    }
    {
        GuestModuleLifecycle m;
        check(m.define(mod("first", 0x1000)) && m.define(mod("overlap", 0x1001)), "define overlapping modules");
        check(m.acquire("first"), "load initial module");
        const auto before = m.initializations();
        check(!m.acquire("overlap"), "reject overlapping module mappings");
        check(m.loadedCount() == 1 && m.initializations() == before && m.opens("overlap") == 0,
              "memory overlap failure leaves state untouched");
        check(m.memory().fetch8(0x1000) == 0x90, "initial module bytes remain intact");
        check(m.release("first"), "cleanly unload surviving module");
    }
    {
        const auto result = runGuestModuleLifecycleDiagnostic();
        check(bool(result), "native lifecycle CPU diagnostic completed");
        check(result && result->modulesLoaded == 3, "three modules linked");
        check(result && result->imports == 1 && result->rax == 42, "CPU executes imported function");
        check(result && result->instructions == 5 && result->stackRestored, "CPU call stack restored");
        check(result && result->initializations == 3 && result->finalizations == 3, "all modules finalized");
        check(result && result->importReadOnly && result->unloaded && result->dependencyProtected,
              "guest import protected and dependencies released");
    }
    {
        MisakiModuleLifecycleReport report{};
        check(misaki_core_run_lifecycle_diagnostic(&report) == 0, "C ABI lifecycle bridge returns success");
        check(report.abi_version == 1 && report.modules_loaded == 3, "C ABI module count");
        check(report.initializations == 3 && report.finalizations == 3, "C ABI init and fini");
        check(report.rax == 42 && report.instructions == 5 && report.linked_imports == 1,
              "C ABI guest CPU execution");
        check(report.unloaded && report.import_read_only && report.dependency_order_valid &&
              report.stack_restored, "C ABI lifecycle invariants");
        check(misaki_core_run_lifecycle_diagnostic(nullptr) == -1, "C ABI rejects null output");
    }
    std::cout << "PASS: " << checked << " lifecycle assertions\n";
}
