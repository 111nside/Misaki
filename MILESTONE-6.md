# Misaki — Milestone 6 (CPU interpreter research subset)

This is a research-only x86-64 userspace test harness targeting iOS. It is **not** a PS4 emulator capable of running the original system UI or games. It contains no PS4 firmware, signing keys, SELF decryptor, PS4 kernel implementation, HLE modules, or GCN/Metal translator.

## New in this update

- Integer x86-64: 64-bit register and memory MOV; ADD, SUB, CMP, XOR, TEST; LEA; immediate groups; REX register extension; register and memory ModRM (RIP relative, SIB, disp8/disp32); indirect CALL, JMP, PUSH.
- Arithmetic: two-operand signed IMUL, one-operand 128-bit-result MUL/IMUL, unsigned DIV, and **limited** signed IDIV (only sign-extended 64-bit dividend). Divide errors become `EmulatorError.divideError`, not host crashes.
- Condition codes: ZF, CF, SF, OF and PF for supported ADD/SUB/CMP/XOR/TEST; all short and near Jcc conditions. Not all x86 EFLAGS are implemented. MUL/IMUL only define CF/OF; division leaves flags unspecified.
- SSE subset: MOVDQU, MOVDQA (alignment checked), PXOR, MOVSS (load), ADDSS and MULSS. XMM0–XMM15 are modeled as 16 bytes each, but full SSE/AVX and floating-point exception semantics are not.
- Experimental cooperative executor: multiple *isolated* guest CPU contexts, round-robin, bounded instruction count. This does **not** support real guest threads, SMP, shared memory, PS4 synchronization or multithreading.
- Toy syscall `0x100` returns 4096 for testing. This is **not** a PS4/FreeBSD syscall.
- New UI: **Execute arithmetic and branch test**. Expected: `Extended CPU test: PASS`, `RAX=42`, `ZF=true`.
- Added 23 Milestone 6 tests and kept 25 existing tests; all 48 pass in a local Linux Swift/XCTest suite. The current GitHub Actions iOS simulator test step should rerun them on macOS.

## Installation via Windows PowerShell

1. Download `Misaki-Milestone6-CPU-SSE.zip` to Downloads.
2. In PowerShell:

```powershell
cd "$HOME\Downloads\Misaki"
git pull --ff-only
Expand-Archive -Path "$HOME\Downloads\Misaki-Milestone6-CPU-SSE.zip" -DestinationPath . -Force
git add .
git commit -m "Add expanded x86 interpreter SSE and guest scheduler"
git push origin main
```

3. Open https://github.com/111nside/Misaki/actions and check that **Run emulator unit tests on iOS Simulator** passes as well as the unsigned IPA build.
4. After signing/installing your test build, open **Execute arithmetic and branch test**.

## Known limitations

- The importer continues to *inspect only* imported ELF headers. It does not run untrusted arbitrary imported programs.
- Only a subset of 64-bit x86 opcodes/addressing is supported; in particular most integer, SIMD and exception opcodes are not available.
- Some instruction forms (including 128-bit signed IDIV) are intentionally unsupported.
- No actual PS4 system software/menu execution yet. The next meaningful compatibility work is a deterministic x86-64 conformance corpus, realistic userspace loader/linker, and properly documented PS4 user-mode ABI/HLE services. Building the actual PlayStation 4 menu remains a very large, separate task.
