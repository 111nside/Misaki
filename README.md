# Misaki

Experimental research project investigating PS4-compatible emulation on iOS. **It cannot boot PS4 firmware, display the genuine PS4 home menu or run PS4 games.**

## Current architecture (Milestone 9)

- **iOS interface:** SwiftUI.
- **Existing executable test engine:** Restricted Swift x86-64 interpreter, ELF loader, module linker, demo kernel API, CPU tests.
- **New portable engine:** C++17 `MisakiNative/` containing bounded ELF metadata validation, guest memory permission checking, guest module and import symbol registry.
- **C ABI:** `MisakiCoreBridge.h` allows Swift to call C++ without requiring Swift-to-C++ interoperability.
- **CI:** Existing iOS simulator tests and unsigned IPA workflow; additional Linux CMake/CTest workflow for fast native-core unit tests.

The native backend is **not** an integration of shadPS4 or any third-party PS4 emulator. No Sony software, firmware or encryption keys are included.

## Windows, Linux or macOS: portable C++ tests

```sh
cmake -S MisakiNative -B build/native -DCMAKE_BUILD_TYPE=Debug
cmake --build build/native
ctest --test-dir build/native --output-on-failure
```

## iOS build

On macOS, install Xcode and XcodeGen; run `xcodegen generate` and open `Misaki.xcodeproj`. The GitHub Actions workflow creates an **unsigned** IPA requiring appropriate signing before installation.

## Longer-term work

1. Integrate portable ELF/module services with the existing guest execution system.
2. Evaluate suitable, legally reusable x86-64-to-ARM64 CPU backends, subject to iOS execution restrictions.
3. Implement PS4-compatible user-mode libraries, syscalls and processes against documented test cases.
4. Develop an independently testable GCN command decoder and Metal backend.
5. Attempt legitimate system-software compatibility and authentic home-menu rendering if technically feasible.

This project currently uses only independently authored test executables. Research builds should be tested with files that you have permission to analyze. If third-party emulator source is later imported, verify and comply with its license before redistribution.
