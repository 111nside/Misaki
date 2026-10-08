# Misaki Milestone 3 — Original PS4 target + ELF execution proof

Target hardware: original PlayStation 4 (CUH-1000). This is a compatibility goal, not a simulated 8 GB RAM/GPU/CPU implementation.

## Changes
- Hardware target constants displayed in the iOS app.
- ELF64 (little-endian, x86-64, ET_EXEC) loader for small, unencrypted test files.
- PT_LOAD segment file mapping, BSS zero filling, entry point validation, input size and mapping limits.
- Existing interpreter can begin execution from ELF e_entry.
- Built-in synthetic ELF demo runs `mov rax, 5; add rax, 3; hlt`; result should be RAX=8.
- Added seven ELF loader unit tests.
- GitHub Actions now runs unit tests on an available iPhone simulator before building an unsigned device app.

## Known limitations
- No PS4 SELF, PRX, firmware, encrypted executable, relocations, dynamic linking, TLS or syscalls.
- CPU interpreter implements only 4 basic instruction forms. No real user-mode process support.
- Memory is a simplistic byte dictionary; no MMU, permissions, paging, or 8GB allocation.
- No PS4 kernel emulation, system apps, user interface rendering, audio, input, GPU or Metal backend.
- The original PS4 menu cannot boot with this milestone.
- The simulator test step needs a Mac runner with an installed iPhone simulator runtime.

## Next milestones toward authentic PS4 menu
1. Memory-region abstraction, access permissions, ELF program headers and error reporting.
2. General x86-64 decode/registers/flags/stack/branches and syscall trap tests using homebrew ELF fixtures.
3. Core kernel HLE process/thread/filesystem/memory primitives and executable linking.
4. Graphics and PS4 system library behavior for independently authored UI test programs.
5. Research system software compatibility and legally sourced firmware requirements.

## Upload
Extract this ZIP into your existing Misaki clone so that `Misaki/`, `MisakiTests/`, and `.github/` merge with existing directories.
Run `git add .`, `git commit -m "Add CUH-1000 profile and ELF execution tests"`, `git push origin main`.
