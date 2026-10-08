# Misaki — Milestone 8: ET_DYN Guest ELF Modules

## Scope

This update adds a bounded dynamic ELF loader for independently authored, unencrypted x86-64 `ET_DYN` test files. It is a research harness and **does not** run PS4 system software, SELF, PRX, or commercial games.

- Load multiple ELF `PT_LOAD` segments with a supplied load bias, including zero-filled BSS.
- Parse `PT_DYNAMIC`, `DT_NEEDED`, SysV `DT_HASH`, `DT_STRTAB`, `DT_SYMTAB`, `DT_RELA`, and `DT_JMPREL`.
- Apply ELF64 RELA types `R_X86_64_RELATIVE`, `R_X86_64_GLOB_DAT`, and `R_X86_64_JUMP_SLOT`.
- Resolve declared imports from explicitly registered guest libraries; expose defined dynamic symbols with their relocated addresses.
- Restore segment read/write/execute permissions after applying relocations.
- Add a new **Modules** tab with a self-contained test that calls an external guest function and returns `RAX=42`.
- Bound file size, symbol count, relocation count, dynamic-string length, and guest address arithmetic.

## Expected in-app diagnostic

```text
ET_DYN module: PASS
RAX=42
Relocations=2
Imports=1
Instructions=5
Stack restored=true
```

## Testing

Locally tested using `swift test` on Linux with all 80 XCTest tests passing (16 tests in `Milestone8Tests.swift`). GitHub Actions should execute the same tests on an iOS simulator and produce its existing unsigned IPA artifact. The iOS build has not been verified for this update yet.

## Limitations

This is **not** a complete dynamic linker. `DT_GNU_HASH`, lazy PLT loading, TLS, ELF symbol versioning, `IRELATIVE`, other relocations, dependency traversal, and most x86-64 instructions are not implemented. It does not emulate the PS4 kernel, system libraries, graphics hardware, or Sony firmware. The hardware target remains the original PS4 CUH-1000.

## Install

Extract this ZIP into the local `Misaki` repository root, commit and push. The ZIP contains only modified and added project files, not a second nested project directory. No changes to the `.github/workflows/ios.yml` workflow are needed.
