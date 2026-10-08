# Misaki — Milestone 7: Guest ELF Linking Prototype

**Original PS4 (CUH-1000) remains the long-term hardware target.** This milestone is a development scaffold, *not a working PS4 loader or shell emulator*.

## Included

- A self-authored x86-64 ELF64 executable with **two loadable segments**: a read/execute code region and a read/write import-pointer (GOT) region.
- A guest symbol registry (`GuestDynamicLinker`) with namespaced library exports and fail-closed, transactional import-pointer patching. Import records are *provided explicitly* by the demo; they are not extracted from a PS4 PRX or from ELF `.dynamic` symbols yet.
- A separate guest library code region mapped read/execute. The ELF test application calls it through a relocated pointer and returns the result `RAX = 42`.
- A limited parser for ELF64 `SHT_RELA` sections supporting **R_X86_64_RELATIVE only**, with bounded inputs and rollback on relocation failure. The relocator is not yet integrated with automatic ET_DYN loading.
- Whole-region `mapZeroed`, `protect`, and `unmap` operations in the sparse guest memory model. The implementation still doesn't model real page tables, partial protection changes, or a PS4 MMU.
- A new **Linker** tab in the iOS app and 12 targeted XCTest cases.

## Use

Merge the contents of this ZIP into the repository root (not into another directory). Push to `main`. GitHub Actions uses the existing simulator test and unsigned-IPA build workflow, which is unchanged by this milestone.

In the app, open **Linker** and press **Execute linked-library test**. Expected:

```
Linked-library test: PASS
RAX=42
Imports=1
Instructions=5
Stack restored=true
```

The five instructions are: indirect CALL, MOV RAX, ADD RAX, RET, HLT.

## Current limitations

**Cannot boot PS4 firmware or the authentic home menu.** No PS4 signed/encrypted SELF or PRX parsing, SCE NID resolving, Sony system libraries, kernel/system-call compatibility, GPU command translation, audio, or controller service implementation. The library in this milestone is a tiny **self-authored guest code stub**, not a host-backed PS4 system API implementation.

The test application is a conventional minimal x86-64 ET_EXEC ELF made in source code. Running an ELF does not imply PS4 binary compatibility. Firmware, license keys, proprietary modules, and game content are not included.

## Suggested next step

Incrementally implement a testable ELF `ET_DYN` loader and per-module load biases; resolve imported symbols from ELF dynamic tables; improve guest memory pages and per-page protections; add early high-level emulation interfaces verified with self-authored test programs. Real PS4 shell boot remains a separate large and uncertain systems project.
