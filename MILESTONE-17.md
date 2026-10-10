# Misaki — Milestone 17: Guest module lifecycle

## Purpose

Add a small, deterministic C++17 guest-module lifecycle manager to Misaki's portable
execution core. **This is not Sony PRX/SELF loading, real PlayStation 4 module
initialization, or firmware booting.** All code fixtures are independently authored.

## Implemented

- Register guest-code module definitions without loading them.
- Follow `needed` dependencies in dependency-first order.
- Reject missing dependencies, dependency cycles and overlapping code mappings
  without changing existing loaded-module state.
- Track explicit open counts and dependency references, including shared dependencies.
- Map immutable guest code RX; reject writes after loading.
- Track initialize/finalize *events* exactly once per load/unload transition,
  with dependencies initialized first and dependents finalized first.
- Remove unused modules by atomically rebuilding the lifecycle-owned immutable
  code mappings (the existing `GuestMemory` does not yet provide unmap).
- Export active module symbols through the existing `ModuleRegistry`.
- Execute a self-authored caller that imports a function from a loaded guest module.
- C ABI and Swift bridge to display the result on the Lifecycle tab.

## Deliberate boundaries

- Initialization/finalization are bookkeeping events, not execution of ELF
  constructors or PRX init/fini functions.
- Only small immutable RX module images are managed. Existing mutable process memory
  and real guest address-space teardown are not handled by this manager.
- Reconstructed mappings are never intended to preserve guest runtime writes.
- No NIDs, Sony system software, commercial executables, GPU emulation or real
  guest dynamic-library ABI is supported.

## Expected result on iPhone

Open **Lifecycle → Execute module lifecycle test**:

    Guest module lifecycle: PASS
    RAX=42
    Modules=3
    Imports=1
    Instructions=5
    Initialized/Finalized=3/3
    Dependency order=true
    Unloaded=true
    Stack restored=true
    Status=0

## Build and test

Extract this overlay ZIP into the existing Misaki repo (retain the existing files),
commit and push. The existing GitHub Actions workflows will execute the entire
native CMake/CTest suite and the iOS Swift/Xcode suite and package an unsigned IPA.

A *standalone* local verification of the new lifecycle component passed 70 assertions
under Clang and GCC and sanitizer checks. That local test used minimal API-compatible
stand-ins for the existing memory/CPU classes, since the full repository could not
be cloned into this environment. The actual full native core and iOS tests therefore
remain to be confirmed by GitHub Actions. The stand-ins are NOT in this ZIP.

## Changed files

- `MisakiNative/Include/GuestModuleLifecycle.hpp` (new)
- `MisakiNative/Source/GuestModuleLifecycle.cpp` (new)
- `MisakiNative/Tests/GuestModuleLifecycleTests.cpp` (new)
- `MisakiNative/Include/MisakiModuleLifecycleBridge.h` (new)
- `MisakiNative/Include/MisakiBridgeAll.h` (new)
- `MisakiNative/Source/GuestModuleLifecycleBridge.cpp` (new)
- `MisakiNative/CMakeLists.txt` (updated)
- `Misaki/Milestone17API.swift` (new)
- `Misaki/Milestone17View.swift` (new)
- `Misaki/MisakiApp.swift` (updated)
- `MisakiTests/Milestone17Tests.swift` (new)
- `project.yml` (updated to import the wrapper C bridging header)

The IPA packaging workflow is not modified.
