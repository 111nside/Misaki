# Misaki Milestones 46–50 — Combined Compatibility Research Update

This overlay extends the portable Misaki CPU, memory and graphics test runtime. It does **not** boot or decrypt PS4 firmware, authenticate SELF files, implement real Sony functions, or execute complete AMD GCN shaders. Sample ELF images, NIDs, services and PM4 command streams are independently authored diagnostic fixtures.

## Included modules

- **46 — `GuestSCE46`:** stages two PT_LOAD segments from an unwrapped self-authored SCE_DYNEXEC ELF64; parses ordinary PT_DYNAMIC / DT_RELA metadata and patches **only** R_X86_64_RELATIVE relocation entries. Uses W^X validation, range checks, restricted budgets and transactional GuestMemory commits. Does not implement SCE-specific relocations, SELF decryption or compressed image loading.
- **47 — `GuestSCE47`:** topologically sorts a small dependency graph, validates exact major/minor versions and synthetic NID keys, binds imports to executable guest addresses and seals slot pages read-only. Detects missing/cyclic dependencies and rolls back on rejection. The export addresses are original Misaki test functions, not real libSce symbols.
- **48 — `GuestVM48`:** page-granular guest address-space facade with map/protect/unmap, region budgets, read-only snapshots and checked host-side writes. Full-region release is supported; partial-page unmap and the PS4 kernel's genuine virtual memory model are not implemented.
- **49 — `GuestGCN49`:** validates the existing nested AMD PM4 render-target metadata, stages Misaki's original software-rendered 2D diagnostic image into guest target memory, then copies and seals a separate presentation buffer. This is **not** execution of GCN draw packets or actual shader rasterization.
- **50 — `GuestSCE50`:** exercises stages 46–49, the existing x86-64 CPU backend and bounded synthetic system services in one guest-memory session; reports five unmet authentic firmware-boot prerequisites rather than attempting an actual boot.

## Validation

`cmake -S MisakiNative -B build/native -DCMAKE_BUILD_TYPE=Debug`

`cmake --build build/native --parallel 2`

`ctest --test-dir build/native --output-on-failure`

The complete project has **41 native test suites**, including five new named tests (`compat46`–`compat50`). New Swift view: **SCE 50**, with one button and independent subsystem PASS indicators. The previous tabs and tests remain intact.

## Expected on-device values

- Five milestones **PASS**, overall **SCE 46–50 PASS**
- `Segments/RELA=2/1`
- `Modules/Imports=2/2`
- `Relocation value=0x103500`
- `Guest RAX=42`
- `Guest memory pages=1`
- `Services/Failures=4/0`
- `PM4 packets/Sprites=11/2`
- `Completed fence=1`
- `Status=0`
- `Real firmware blockers=5`
- `Firmware boot attempted=false`
- `Original home menu booted=false`

## Installation

Apply the archive at the **Misaki repository root**, not within `Misaki/` or `MisakiNative/`. The GitHub Actions workflows are unchanged. Verify both **Test Misaki Native Core** and **Build Misaki iOS** after pushing. The device-side SwiftUI and Metal build has not been run in this Linux environment.
