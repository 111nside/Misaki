# Misaki Milestone 13 — Guest services and cooperative scheduling

This is an overlay for `111nside/Misaki` **after Milestone 12**. The existing GitHub Actions IPA packaging and previous diagnostics are preserved.

## Added functionality

- A small, opt-in demonstration ABI for the C++ x86-64 backend. `0F 05` (SYSCALL) reaches a **guest service dispatcher** rather than making a host OS call.
- Five strictly synthetic services, using RAX to select a service, RDI as the first argument, and RSI as the second:
  - `0x100`: return guest page size (4096)
  - `0x101`: validate and copy at most 4096 bytes from readable guest memory to a bounded diagnostic output buffer
  - `0x102`: yield cooperatively
  - `0x103`: record a guest exit status and stop the guest
  - `0x104`: return a deterministic guest thread ID
- Bounded round-robin scheduling of up to eight **independent** guest CPU/memory contexts. This is an execution-context simulator, **not** shared-memory pthreads or multithreaded host execution.
- Unsupported services and memory faults stop the test guest safely. Bounded execution and output limits prevent runaway fixtures.
- A new `Core` tab diagnostic: **Execute guest services and threads**. It loads two original ELF64 fixtures through `loadGuestELF`, executes 30 instructions in total, captures `OKOK`, and reports each context's RAX and restored stack.
- Added C bridge, Swift API and XCTest regression check. CMake/CTest now contains a fourth native test suite.

## Expected result on device

```text
Guest services and threads: PASS
Threads=2
Instructions=30
Service calls=8
Yield events=2
Guest writes=2
Output=OKOK
RAX1=4097, RAX2=4098
Stacks restored=true
Status=0
```

## Apply on Windows PowerShell

```powershell
cd "$HOME\Downloads\Misaki"
git pull --ff-only
Expand-Archive -Path "$HOME\Downloads\Misaki-Milestone13-Guest-Services.zip" -DestinationPath . -Force
git add .
git commit -m "Add bounded native guest services and cooperative scheduling"
git push origin main
```

Watch the `Test Misaki Native Core` and `Build Misaki iOS` workflows. The latter still uploads an **unsigned** IPA, which requires signing before an iPhone can install it.

## Limits / next architectural work

This is **not** the PS4 syscall ABI, SCE kernel service emulation, actual PS4 scheduling, executable SELF/PRX handling, Sony firmware booting, or GPU translation. All guest programs in the tests are original synthetic ELF64 fixtures. Next work could add a process-level handle table, a virtual guest filesystem and shared-memory/thread synchronization primitives, while maintaining strict sandbox boundaries.
