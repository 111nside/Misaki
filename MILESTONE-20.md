# Misaki Milestone 20 — Guest-Memory GPU Command Queue

**Scope:** Original, bounded 2D command-queue prototype, not an implementation of Sony GNM/GNMX, AMD GCN graphics, the PS4 kernel or the original PS4 home menu.

## Architecture

1. `GuestGraphics20.cpp` builds a **self-authored x86-64 guest program**, executed using the existing `PortableX64Backend`.
2. A 7-record drawing list (32 bytes per command) is mapped into the guest process' **readable/writable virtual memory** at `0x5000`. The guest changes the sprite position by executing `MOV [RDI], RAX` against the mapped packet list.
3. The guest executes a **synthetic** `SYSCALL` with RAX `0x340`, RDI=guest queue pointer, RSI=command count, and RDX=monotonic submission fence. This is neither an iOS system call nor a PS4 syscall number.
4. `GuestGraphicsQueue` validates bounds, guest readability, record kinds, command ordering, scene coordinates, texture references, and frame data. It **copies** validated command data and rendered RGBA data out of guest memory, with no leaked host pointers.
5. Two queued frame snapshots are allowed. Fence numbers must strictly increase. `take()` completes a frame synchronously and signals its fence. This does not model asynchronous real GPU execution.
6. The existing G19 rasterizer produces the 320×180 RGBA8 framebuffer; `misaki_gpu20_render` exports a transactional, capacity-checked C ABI; `MetalGraphics20Canvas` renders it with the established two-pass Metal shader approach.

## On-device verification

Open **Graphics → Execute guest GPU command queue**. For the static first frame:

```
Guest command queue: PASS
Canvas=320x180
Commands=7
Rectangles=4
Textured sprites=1
Guest instructions=9
Guest GPU calls=1
Sprite X=32
Submitted/Completed fence=1/1
Queue drained=true
Status=0
```

A textured patterned sprite should be visible. Tap **Start animation**; it moves across the navy scene in a 90-frame cycle at approximately 12 FPS. Milestone 19 remains accessible as **Textures**, and Milestone 18 as **Basic GPU**.

## Tests and limitations

- `GuestGraphics20Tests.cpp`: 74 C++ assertions check guest execution, framebuffer consistency, stale and duplicate fences, 2-slot queue capacity, snapshot independence, unreadable memory, malformed packets, and C ABI buffer bounds.
- The existing ten native test suites continue to run. `MisakiTests/Milestone20Tests.swift` checks the new C bridge on iOS Simulator.
- Linux verification: CMake/CTest, Clang/GCC cross-check if available, AddressSanitizer + UndefinedBehaviorSanitizer, and Swift source/bridge type-checks. **The complete Xcode/iOS build must still be verified by GitHub Actions.**
- No actual PS4 GPU packet parsing, shader translation, real firmware execution, or home-menu boot is implemented.

## Apply on Windows PowerShell

```powershell
cd "$HOME\Downloads\Misaki"
git pull --ff-only
Expand-Archive -Path "$HOME\Downloads\Misaki-Milestone20-Guest-GPU-Queue.zip" -DestinationPath . -Force
git add .
git commit -m "Add guest-memory GPU command queue and Metal diagnostic"
git push origin main
```

Check both **Test Misaki Native Core** and **Build Misaki iOS** under GitHub Actions. The normal unsigned IPA packaging workflow is unchanged.
