# Misaki Milestone 10 — Native guest execution

This is the first step toward executing test guest x86-64 machine code in the portable C++ engine rather than only in the Swift interpreter.

## Features

- Loads small, ordinary x86-64 ELF64 `PT_LOAD` segments into guest memory and applies final `R/W/X` permissions.
- Supports `ET_EXEC` and load-biased `ET_DYN` images in the native test ELF loader.
- Executes a **strict subset** of x86-64 instructions in a bounded, non-JIT C++ interpreter: `MOV RAX,imm64`, `ADD RAX,imm32`, `CMP RAX,imm32`, `CALL rel32`, `CALL [RIP+disp32]`, `RET`, `NOP`, `HLT`, `JE` and `JNE` rel8.
- Has a guest stack and traps unsupported instructions, invalid execution addresses and over-budget execution.
- Executes a self-authored ELF64 program that calls a separately mapped test function via a guest import pointer. Expected `RAX=42`, `Instructions=5`, stack restored.
- Exposes that diagnostic through an additive C ABI (`misaki_core_run_cpu_diagnostic`) and Swift API.
- Preserves the older Swift interpreter and Milestones 1–9, and does **not** change `.github/workflows/ios.yml`.

## Verification performed

- CMake/CTest: portable core unit suite passes (87 assertions total).
- AddressSanitizer and UndefinedBehaviorSanitizer run: passes.
- Clang C++17 build and tests: passes.
- Linux Swift command-line bridge smoke test: old and new C ABI diagnostics both pass.
- SwiftUI view: Swift frontend syntax parser passes. **An actual iOS/Xcode build has not been run locally**; GitHub Actions will be the iOS verification.

## Expected iPhone diagnostic

1. Go to **Core** tab.
2. Tap **Execute native guest ELF**.
3. Verify output:

```
Native guest CPU: PASS
RAX=42
Instructions=5
Imports=1
Stack restored=true
Read-only import=true
Status=0
```

The existing **Run native core diagnostic** button should also still pass.

## Limitations and next steps

This update **does not run PS4 firmware or the real PS4 home menu**. It does not import shadPS4, emulate AMD Jaguar hardware, implement Sony PRX/SELF/NIDs, or offer a GPU/Metal backend. It supports deliberately small, independent test binaries and is not a full x86-64 interpreter. Larger speed gains require carefully selecting a mature x86-64 CPU engine for ARM64/iOS and integrating genuine PS4-compatible userspace, not just adding opcode cases to this test harness.
