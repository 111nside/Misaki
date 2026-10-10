# Misaki Milestone 21 — Render states, two textures, and Metal shader effects

This is an **overlay update for a working Milestone 20 checkout** of `111nside/Misaki`. It does not include the full repository or a compiled IPA. It is a research-only emulator prototype and does not run PlayStation 4 firmware or games.

## Features

- Guest-owned 32-byte GPU packets in x86-64 virtual memory, with an original six-opcode protocol (clear, fill, sprite, present, state, scissor).
- Two independent procedural RGBA textures, alpha and additive blending, opaque tint modulation, and clip/scissor rectangles.
- C++ renderer validates an entire guest packet stream before producing a 320×180 RGBA framebuffer. Bad commands do not commit fences or partially modify C bridge outputs.
- Guest x86-64 program patches two sprite positions and the shader effect in its packet stream, then submits via a **private, synthetic** guest service `0x350`.
- A 2-entry bounded FIFO guest command queue with monotonic submission and completion fences; each queued frame owns an independent copy of commands and pixels.
- SwiftUI **Graphics** tab with mode selection (Normal, Grayscale, Invert, Scanlines) and moving sprites, rendered with Metal in two passes (RGBA framebuffer -> offscreen texture -> display shader).
- Prior **Queue** (Milestone 20), **Textures** (19), and **Basic GPU** (18) diagnostics remain available.

## Apply from Windows PowerShell

Save `Misaki-Milestone21-Render-State-Metal.zip` to `Downloads` and run:

```powershell
cd "$HOME\Downloads\Misaki"
git pull --ff-only
Expand-Archive -Path "$HOME\Downloads\Misaki-Milestone21-Render-State-Metal.zip" -DestinationPath . -Force
git add .
git commit -m "Add guest GPU render states and Metal shader effects"
git push origin main
```

Visit https://github.com/111nside/Misaki/actions to confirm both **Test Misaki Native Core** and **Build Misaki iOS** succeed. Sign the resulting unsigned IPA in your normal setup.

## Test on an iPhone

1. Open the **Graphics** tab and choose **Render guest graphics pipeline**.
2. Verify: `PASS`, `Canvas=320x180`, `Commands=10`, `Textures=2`, `Sprites=2`, `Blend state changes=2`, `Scissor changes=2`, `Guest instructions=15`, `GPU calls=1`, `Sprite X=32, 208`, `Submitted/Completed=1/1`, `Queue drained=true`, `Status=0`.
3. Two patterned sprites should be visible on a navy frame. Tap **Start animation** to move them toward each other.
4. Switch the **Shader effect** selector and verify Normal, Grayscale, Invert and Scanlines change the Metal output.
5. Check the **Queue** tab to make sure Milestone 20 still works.

## Verification performed locally

- GCC debug: all 12 CTest suites pass.
- Clang release: all 12 CTest suites pass.
- AddressSanitizer + UndefinedBehaviorSanitizer: all 12 CTest suites pass.
- Milestone 21 unit test binary: 114 native assertions pass.
- Swift file syntax checks pass; the new Swift/C ABI call is type-checked with matching local C declaration stubs.
- A frame from the actual C++ renderer was inspected visually.

The complete XcodeGen project, Metal shaders and iOS Simulator/device builds cannot be compiled in the local Linux environment. GitHub Actions provides that final check.

## Limitations

The compositing and scissor operations are currently performed by a portable **software** rasterizer, and the finished frame is displayed through **Metal** using shader effects. This is intentionally **not** an AMD GCN packet decoder, GPU shader ISA translator, Sony GNM/GNMX, or a PS4 system UI renderer. There is no promise that the original PS4 home menu can boot.
