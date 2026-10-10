# Misaki Milestone 15 — Integrated guest system-library pipeline

## What works

- A self-authored, standard x86-64 `ET_DYN` test module is loaded at guest bias `0x4000`.
- Its `PT_DYNAMIC` descriptors and symbol table are parsed by the existing native linker.
- `R_X86_64_RELATIVE` and `R_X86_64_JUMP_SLOT` relocations are applied.
- The dynamically imported symbol resolves to an independently mapped, guest-executable library at `0x9000`.
- The C++ x86-64 backend executes the guest library's *instructions* (no C++ host-function shortcut).
- The library opens `/system/message.txt`, reads five bytes, writes the guest buffer, closes the descriptor, queries guest PID/page size, and returns.
- Output is `Hello`, `RAX=42`, 26 executed instructions, one import, one relative relocation, and a restored guest stack.
- Guest code/GOT mappings are protected against writes. All file contents live only in an in-memory guest virtual filesystem.
- A Core-tab diagnostic and Swift/XCTest bridge test expose the end-to-end result.

## Deliberate limitations

This is a **private, original test ABI**, not Sony's kernel/syscall interface, SCE libraries, NIDs, PRX/SELF decoding, real device file access, or a working PS4 operating system. The test file and library are independently authored and contain no Sony software. ET_DYN support remains restricted to small, carefully controlled ELF64 fixtures. The CPU interpreter is an incomplete subset and does not yet provide usable PS4 game or firmware emulation.

The `libMisakiDynamic::increment2` symbol name is inherited from the Milestone 12 test fixture. The new library implementation demonstrates VFS calls before returning 42; the symbol is not a real PS4 export.

## Local verification

- `cmake -S MisakiNative -B build/native && cmake --build build/native --parallel 4 && ctest --test-dir build/native --output-on-failure`
- Six CTest suites pass under GCC 14 and Clang 17.
- The new integration suite passes 51 assertions, including malformed ELF, unresolved imports, rollback, isolated in-memory VFS, and C ABI tests.
- Clang AddressSanitizer/UndefinedBehaviorSanitizer tests pass.
- Swift bridge typechecks using a C module map; Swift UI source parses.

**The complete Xcode/iOS build is not locally verified; check GitHub Actions.**

## iPhone diagnostic

Core tab → **Execute linked system-service ELF**

Expected output:

```
Guest system-library test: PASS
RAX=42
Instructions=26
Imports=1
Relative relocations=1
Open/Read/Close/Write=1/1/1/1
Guest output=Hello
PID=1001
Stack restored=true
Read-only import=true
Status=0
```

## Installation

Unzip the milestone overlay over your local `Misaki` repo, commit, and push. It preserves the current GitHub Actions unsigned IPA packaging and previous tests.
