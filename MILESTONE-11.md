# Milestone 11 — Expandable native x86-64 backend

Adds a **second, portable, non-JIT execution backend** for independent x86-64 test programs.
The old Milestone 10 guest executor is preserved.

## Implementation

- `IX64ExecutionBackend`: interface intended to make a future optimized implementation replaceable.
- `PortableX64Backend`: 16 GPRs, REX extensions, 64-bit register and memory operands, SIB, displacement, RIP-relative addressing, basic flags and conditional branches, CALL/RET, stack operations, IMUL, memory protection enforcement, and bounded execution.
- Small linked-library diagnostic: self-authored guest function, register arithmetic and conditional flow, import binding, and restored stack.
- New fast CMake test executable alongside the original native tests.
- C ABI / SwiftUI Core tab diagnostic and iOS XCTest coverage.

## On-device expected diagnostic

Open **Core** and tap **Execute expanded CPU backend**.

```
Expanded CPU backend: PASS
RAX=44
Instructions=13
Imports=1
Stack restored=true
Import read-only=true
Status=0
```

## Scope and limitations

This is **not** an implementation of shadPS4, a PS4 CPU, PlayStation SELF/PRX loading, a kernel, GPU, or firmware compatibility. It intentionally does not include JIT, SSE/AVX, TLS, native PS4 system calls, or optimized x86-to-ARM64 translation. Its instruction set is limited to the cases documented by the tests. No protected firmware, games, keys, or Sony libraries are included.

## Testing

`cmake -S MisakiNative -B build/native -DCMAKE_BUILD_TYPE=Debug`

`cmake --build build/native --parallel 2`

`ctest --test-dir build/native --output-on-failure`

A separate Swift XCTest verifies that the expanded backend is callable from the iOS UI. iOS compilation and signing must still be checked in GitHub Actions.
