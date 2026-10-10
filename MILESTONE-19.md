# Misaki Milestone 19 — Textures, Offscreen Rendering and Guest CPU → GPU

**Scope:** Demonstration of Misaki's original, non-PS4 GPU command protocol.
The software rasterizer is not an AMD GCN interpreter or Sony GNM/GNMX implementation.

## Added

- `MisakiNative/Include/GuestGraphics19.hpp`, `Source/GuestGraphics19.cpp`: validated texture and rectangle command stream; bounded 320×180 RGBA8 framebuffer; alpha blending; deterministic frame hashes.
- Guest x86-64 instruction stream: MOV RDI, MOV RAX, MOV [RDI], MOV RAX, SYSCALL, HLT. The guest writes a sprite X-coordinate into guest memory, then makes a **synthetic** graphics service call (ID `0x330`). The service validates a guest descriptor and submits one sprite draw. Zero real iOS or PS4 syscalls.
- `MisakiGPU19Bridge.h`, `GuestGraphics19Bridge.cpp`: capacity-checked Swift-compatible C interface. No partial pixel writes on error.
- `Misaki/NativeGraphics19API.swift`: typed RGBA frame and diagnostic.
- `Misaki/MetalGraphics19Canvas.swift`: first Metal pass samples the RGBA8 texture into a reusable private BGRA8 offscreen render target; second pass samples that framebuffer into the drawable with a subtle programmable color-gain shader.
- `Misaki/Milestone19View.swift`: render once, then optionally animate at approximately 12 FPS with a 90-frame repeating path. The old basic GPU diagnostic remains under **Basic GPU**.
- Native/Swift tests and native CTest registration.

## Expected result

Open **Graphics → Render guest textured frame**:

```
Textured GPU: PASS
Canvas=320x180
Commands=7
Rectangles=4
Textured sprites=1
Guest instructions=6
Guest GPU calls=1
Sprite X=32
Status=0
```

The colored sprite appears in a navy panel. Press **Start animation** to move it across the frame. The status text describes the initial frame; the preview advances while the animation runs.

## Build and test

```bash
cmake -S MisakiNative -B build/native -DCMAKE_BUILD_TYPE=Debug
cmake --build build/native --parallel 2
ctest --test-dir build/native --output-on-failure
```

GitHub Actions builds the iOS app, simulator XCTest, and unsigned IPA. Source code is deliberately isolated from earlier milestones.

## Limits

- The native rasterizer produces guest pixels on the CPU; Metal samples, composites and presents these textures via two graphics passes.
- This is **not** a PS4 GPU or a graphics driver usable by real firmware.
- No Sony firmware, keys or game code are bundled or required.
