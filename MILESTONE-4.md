# Misaki Milestone 4 — demo userspace execution

## Goal

Prove that Misaki can load an independently-authored x86-64 ELF, execute a small series of CPU instructions, intercept a **toy/demo ABI** `syscall` instruction, emit text, and exit. This is not PS4 firmware, PS4 kernel emulation, or a working PS4 emulator.

## Additions

- Expanded REX.W 64-bit register handling (all 16 register encodings for immediate `MOV`); selected register-register `MOV` and `XOR` instructions.
- `ADD`, `SUB`, `CMP` on RAX with sign-extended imm32 (only zero flag modeled).
- `JMP`, `JE` and `JNE` relative branches.
- `syscall` for **demo-only** call numbers `1` (text output using RDI/RSI) and `60` (exit using RDI).
- Instruction-count limit and bounded output reads, to help constrain test programs.
- Diagnostic app button and XCTest coverage.

## Boundaries

**The demo ABI is NOT the PS4's syscall ABI.** No PS4 firmware, secure loading, retail games, kernel, GPU, graphics, JIT or system libraries are implemented. Importing user ELF files in Misaki continues to inspect headers only, not execute them. The loader handles a tightly limited ELF subset and the virtual memory model has no MMU or page permissions.

## Validate

Push these files, open GitHub Actions > Build Misaki iOS, and check `Run emulator unit tests on iOS Simulator` as well as both builds. In the app, tap `Execute demo userspace ELF`. It should print `Misaki!`, exit code 0, and 7 instructions executed. The existing `Execute built-in ELF test` should still produce RAX=8.

## Next milestones toward authentic PS4 menu

1. More capable x86-64 decoding/interpreter and memory protection, exception handling, stack/call support.
2. Userland module loading, relocation and linking, and independently authored service stubs.
3. Investigate lawful access and technical compatibility of real system software, boot dependencies and graphics.
4. Metal graphics translation and input/audio integration.

The real PS4 menu is a far harder project, with no guaranteed ability to boot commercial system firmware on iOS.
