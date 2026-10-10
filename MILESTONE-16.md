# Misaki — Milestone 16: Versioned test library catalog and RELA expansion

## Purpose

Move beyond one-off guest library declarations by adding a bounded, versioned
catalog of *independently authored* symbols and covering additional ordinary
ELF64 x86-64 relocations.

**Scope:** A non-JIT userspace research prototype; **NOT** the PlayStation 4
system library manager, Sony NID resolver, SELF/PRX loader, PS4 kernel, or
firmware boot support. It cannot launch the genuine PS4 home menu.

## New components

- `MisakiNative/Include/GuestLibraryCatalog.hpp` and
  `MisakiNative/Source/GuestLibraryCatalog.cpp`: versioned per-library symbol
  catalog. Supports registration, duplicate rejection, pinned-major lookup,
  minimum compatible minor version, deterministic highest-version selection,
  and a bounded snapshot for the existing ELF importer.
- `GuestDynamicExecution.cpp`: adds `R_X86_64_64`, `R_X86_64_32`,
  `R_X86_64_32S`, `R_X86_64_PC32`, and `R_X86_64_PLT32` in addition to existing
  `RELATIVE`, `GLOB_DAT`, and `JUMP_SLOT` relocations. Validates arithmetic
  width, displacement sign, slot overlap, table bounds, write permissions, and
  all-or-nothing mapping/relocation behavior.
- C/Swift bridge and Core tab diagnostic.
- `GuestLibraryCatalogTests.cpp`: positive execution, version selection,
  missing symbols, overflow, protection faults, malformed input, negative
  relocation values, rollback, and C ABI tests.

## How to use

From Windows PowerShell, in a clean checkout at `$HOME\Downloads\Misaki`:

```powershell
cd "$HOME\Downloads\Misaki"
git pull --ff-only
Expand-Archive -Path "$HOME\Downloads\Misaki-Milestone16-Library-Catalog.zip" -DestinationPath . -Force
git add .
git commit -m "Add versioned guest library catalog and ELF relocations"
git push origin main
```

GitHub Actions should run both the native CMake tests and the iOS simulator
unit tests, then generate the same **unsigned** IPA artifact.

On iPhone: **Core → Execute versioned library and relocation test**.

Expected:

```
Guest library catalog: PASS
RAX=42
Instructions=5
Catalog versions=2
Imports=6
Relative/Absolute/PC-relative=1/3/2
ABS64=0x8ff8
PC32=16076
Stack restored=true
Read-only imports=true
Status=0
```

Note: `PC32=16076` is decimal `0x3ECC`. The diagnostic applies seven
relocations overall (one `RELATIVE` plus six symbol imports), but still executes
only five CPU instructions because the extra relocated slots are test data.

## Local validation

- All **7 CMake/CTest suites** passed.
- **70 new native library-catalog assertions** passed.
- All seven native suites passed with Clang AddressSanitizer and
  UndefinedBehaviorSanitizer.
- The Swift API passed type-checking against the C bridge on Linux; the SwiftUI
  code passed syntax parsing. The full iOS compile must still pass GitHub Actions.

## Not implemented

This catalog does not parse ELF `VERSYM/VERNEED/VERDEF` metadata, import
Sony/SCE modules, resolve PS4 NIDs, execute a PS4 kernel, translate GCN
commands, or bypass iOS code signing/JIT restrictions.
