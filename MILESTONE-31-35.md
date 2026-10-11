# Misaki Phase 2: Milestones 31–35 combined

This package is an **overlay** for the existing working Milestone 30 `main` branch.
It contains new and modified files only, not an entire replacement repository.

## One shared, bounded diagnostic runtime

- **31 — Cooperative runtime:** `IntegratedGuestRuntime` schedules up to four separate guest x86-64 processes. Each has its own `GuestMemory` copy, registers, guest stack, VFS descriptor table, and service dispatcher. Round-robin execution has instruction and quantum limits.
- **32 — GPU submission/fences:** A guest process writes a sprite coordinate into its own virtual-memory command buffer and issues simulated services `0x510` (submit), `0x511` (poll), and `0x512` (wait). Queued frames are copied/validated by the existing `GuestGraphics21Queue`. Waiting advances a *software* completion fence. The GPU queue still rasterizes synchronously on the CPU, **not** via an asynchronous PS4 hardware queue.
- **33 — GCN in process context:** Simulated services `0x513` and `0x514` read protected guest-memory buffers to execute the **existing limited** fused GCN single-lane evaluator and recursively inspect selected AMD PM4 commands. This connects Milestones 23 and 30 to the new process runtime; it does not add full GCN wavefront semantics, real graphics queues, or native PS4 GPU execution.
- **34 — Guest loader/modules:** A second runtime process is created by `loadAndLinkDynamicELF()` using an ordinary, authored `ET_DYN` ELF fixture. `GuestModuleLifecycle` resolves an imported function and its dependency, and x86-64 execution returns RAX `42`. This is **not** SELF/PRX, Sony NID linking, PS4 binary execution, or firmware compatibility.
- **35 — Integrated services:** In a single guest execution, the process opens/reads/closes an immutable virtual file, submits and waits on a graphics frame, inspects nested PM4, and runs the bounded fused GCN reference. It never accesses iOS host filesystem through guest pathnames, and its service numbers are **not** PS4 kernel syscalls.

## Run tests independently

```bash
cmake -S MisakiNative -B build/native -DCMAKE_BUILD_TYPE=Debug
cmake --build build/native --parallel 2
ctest --test-dir build/native --output-on-failure
# Five individually runnable subsystem suites:
ctest --test-dir build/native -R 'integrated-31-subsystem' --output-on-failure
ctest --test-dir build/native -R 'integrated-32-subsystem' --output-on-failure
ctest --test-dir build/native -R 'integrated-33-subsystem' --output-on-failure
ctest --test-dir build/native -R 'integrated-34-subsystem' --output-on-failure
ctest --test-dir build/native -R 'integrated-35-subsystem' --output-on-failure
```

As developed in the local container: **26/26** CTest suites pass (the original 21 plus five new suites), and **140** new assertions pass (34, 35, 34, 14, 23). The complete suite also passes under Clang with AddressSanitizer/UndefinedBehaviorSanitizer. New Swift API type-checks against the existing C bridge, and Swift sources pass parser checks. **Full iOS compilation and on-device Metal drawing require GitHub Actions and iPhone verification.**

## Expected on-device diagnostic

In **Runtime** (possibly under **More**), tap **Execute integrated guest runtime**.

```
Integrated guest runtime: PASS
Processes=2
Instructions=41
Context switches=11
VFS open/read/close=1/1/1
GPU calls=1
PM4 packets=11
GCN shader operations=11
Loaded modules / imports=2/1
Completed fence=1/1
ELF RAX=42
Guest PID=1001
GCN output record 7=31
Status=0
```

Each milestone appears individually as PASS/FAIL. The displayed framebuffer contains the existing two synthetic textured sprites, drawn using the Metal presentation path. **There is no authentic PS4 boot screen** and no promise that PS4 firmware will run after this package.

## Installation on Windows PowerShell

```powershell
cd "$HOME\Downloads\Misaki"
git pull --ff-only
Expand-Archive -Path "$HOME\Downloads\Misaki-Milestones31-35-Integrated-Runtime.zip" -DestinationPath . -Force
git add .
git commit -m "Integrate Misaki milestones 31-35 runtime"
git push origin main
```

After pushing, check both **Test Misaki Native Core** and **Build Misaki iOS** workflows. The GitHub Actions IPA is unsigned and requires your usual signing/sideloading procedure.
