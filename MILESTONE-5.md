# Misaki Milestone 5: CPU stack and memory permissions

Target: original PS4 CUH-1000, eventually running its real home menu.

## What changes

- Guest stack mapping: a **64 KiB prototype stack** with readable/writable, not executable permissions.
- CPU instruction forms: PUSH/POP (selected registers and immediates), CALL rel32, RET.
- Guest stack pointer (RSP) and RCX become observable in diagnostics.
- ELF PT_LOAD flags are mapped to read/write/execute permissions. Writes to code and execution from non-executable guest memory now fail.
- The new self-authored `DemoStackELF` calls a small function and returns, producing RAX=8 and RCX=5.
- Adds XCTest coverage for call/return, stack restoration, sign extension and memory protections.

## Limitations

This is still **not a PS4 system software loader**. It does not boot the home menu or run commercial games. It does not implement kernel services, the PS4's SELF/PRX packaging, modern x86 instruction coverage, dynamic linking, Metal GPU translation or PS4 firmware.

The original debug ABI in Milestone 4 remains unchanged and is **not a PS4 syscall ABI**.

## Install

From a Windows PowerShell prompt:

```powershell
cd "$HOME\Downloads\Misaki"
git pull --ff-only
Expand-Archive -Path "$HOME\Downloads\Misaki-Milestone5-StackMemory.zip" -DestinationPath . -Force
git add .
git commit -m "Add x86 stack calls and memory protections"
git push origin main
```

The archive contains changed/new files only. It keeps the existing project settings and CI workflow.
