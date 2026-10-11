# Misaki mega update: Milestones 56–60

This additive patch applies on top of the **Milestone 51–55** working `main` branch. It adds five independent CTest suites and one combined iPhone diagnostic, without removing prior modules or Metal demonstrations.

## Implemented

- **56 — ELF dynamic dependency inspector:** parses bounded unwrapped SCE ELF64 `PT_DYNAMIC`, checks file-backed `DT_STRTAB`/`DT_STRSZ`/`DT_NEEDED`, validates a standard ELF `DT_RELA` description, reports SCE module identifiers, refuses malformed ranges and duplicate names. File contents are never executed by this inspector. SELF remains inspection-only.
- **57 — synthetic service registry:** version- and NID-identity-checked registration of two self-authored guest services. It routes only to Misaki's bounded in-memory shim and denies cross-process calls. No Sony system-call IDs or real exports are implemented.
- **58 — isolated GPU surface buffer copying:** per-process 16×16 RGBA8 software surface copy through the GuestKernel53 guest-memory manager, with mapped permissions, bounds checks, strict fence ordering, and no partial writes. This is **not** AMD GCN graphics execution.
- **59 — persistent CPU session:** two self-authored x86-64 guest processes resume across separate scheduler ticks using the existing portable CPU backend and four private test services (PID, yield, clock, exit).
- **60 — read-only user-file compatibility inspection:** imports an executable via iOS Files and analyzes up to **8 MiB** of header bytes, reporting recognized SELF/SCE-ELF/ordinary ELF structure, encrypted/compressed segments, eligible DT_NEEDED names, incomplete inspection, and actionable blockers. It does not decrypt, load, or run imported data.

## Test results (local)

- **51 / 51 CTest suites passed** in a normal CMake native build.
- **5 / 5 new suites passed** under Clang AddressSanitizer and UndefinedBehaviorSanitizer; instrumented rebuild had to be resumed after the time budget, then completed.
- `NativeMega60API.swift` type-checked with the actual project C ABI header. New iOS SwiftUI files parsed, but the **complete iOS app has not been built on macOS here**. Check GitHub Actions.

## iPhone steps

Open **PS4 Analyzer** (possibly under **More**), tap **Run five-subsystem diagnostic**. Expected:

```
Milestones 56–60: PASS
ELF dependencies / RELA=1/1
Guest services / rejected=3/1
GPU pixels / copies=256/1
Processes / exits=2/2
Guest instructions=16
Encrypted SELF segments=1
Real firmware blockers=5
Boot attempted=false
Home menu booted=false
Status=0
```

Then tap **Choose SELF / SPRX / ELF file**. A user-provided file will be classified locally with a report describing recognized metadata and blockers. Supported headers are not proof that an application can run. If the file exceeds 8 MiB, the report explicitly identifies partial inspection.

## Current limits

**No genuine PS4 firmware boot**, PS4 home menu, Sony SELF decryption, authentic `libSce` implementations, complete kernel, GPU, shader compiler, or retail PS4 title execution. A built-in regression result does not assert compatibility with an arbitrary user file. Do not mistake the self-authored examples for a signed or encrypted retail PS4 executable.

The parser exposes user-declared dependency names; it does not verify their availability. A dynamic table not recognized by the small supported subset is reported conservatively as unsupported.
