# Milestone 9 — Portable C++ Core (accelerated development)

This release begins a cross-platform C++17 backend and keeps the previous Swift engine intact.

## What changes

- `MisakiNative/`: native C++ library with bounded ELF64 ET_EXEC/ET_DYN inspection, permission-checked guest memory and a guest module-symbol registry.
- Stable `extern "C"` API in `MisakiCoreBridge.h` so Swift can call the C++ core without Swift/C++ interoperability being required.
- New **Core** tab and two iOS bridge tests.
- A dedicated **Test Misaki Native Core** GitHub Actions workflow using Linux/CMake/CTest. This lets native C++ tests complete independently of the iOS IPA build. The existing iOS workflow is unchanged.

This is **not** a port of shadPS4, and not yet an implementation of PS4 kernel, Sony libraries, GPU or firmware. No third-party emulator code is copied into this update. shadPS4's GNU GPL-2.0 licensing must be reviewed before incorporating its source.

## Build and test locally (Windows, macOS, Linux)

From the repository root:

```sh
cmake -S MisakiNative -B build/native -DCMAKE_BUILD_TYPE=Debug
cmake --build build/native
ctest --test-dir build/native --output-on-failure
```

## iOS integration

`project.yml` includes the C++ sources and sets `SWIFT_OBJC_BRIDGING_HEADER` to `MisakiNative/Include/MisakiCoreBridge.h`. XcodeGen must regenerate the `.xcodeproj` (the GitHub Actions iOS workflow already regenerates it). The Swift Core tab invokes `misaki_core_run_diagnostic` through the C API.

Expected Core tab after running diagnostic:

```
Native core: PASS
ELF type=3
Modules=1
Imports=1
Guest pointer=0x6200
Read-only=true
Status=0
```

## Limitations and next integration step

The new C++ code validates synthetic ELF metadata and fills an import pointer, but **does not execute guest instructions**. The existing Swift x86-64 interpreter remains the only current execution engine.

The next step is to share verified guest module metadata with the Swift ELF loader, using a single stable boundary, then progressively migrate or integrate an established, appropriately licensed CPU backend rather than maintaining two independent emulation engines.
