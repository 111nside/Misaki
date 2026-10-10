# Milestone 18 — virtual GPU commands and Metal output

First **actual visual output** on an iPhone using an original C++ command buffer and Apple's Metal API. This is intentionally not PS4 GPU emulation.

## Architecture

- `GuestGPU` (portable C++17): bounded command stream with clear/filled-rectangle/present, dimensions and rectangle validation, ordered commands and cross-platform FNV-1a checksum.
- `MisakiGPUBridge.h` (stable C ABI): copies a validated frame to Swift, with capacity checks and no partial output writes on failure.
- `NativeGPUAPI.swift`: decodes C commands and verifies a fixed synthetic test frame.
- `MetalGuestCanvas.swift`: `MTKView` with a tiny inline Metal shader; a real Metal pipeline draws the guest rectangles on iOS. No shaders from PS4 binaries are used.
- `Milestone18View.swift`: the Graphics tab, button and visual frame.
- `GuestGPUTests.cpp`: fast portable native tests, also run by `native-core.yml` through CMake/CTest.
- `Milestone18Tests.swift`: iOS integration tests.

## Run

Select **Graphics → Render guest GPU frame**.

Expected text: `GPU command stream: PASS`, `Canvas=320x180`, `Commands=7`, `Rectangles=5`, `Clear/Present=1/1`, `Status=0`.

You should see a navy frame with five colored rectangles. The screenshot is just an original diagnostic composition, **not** the real PlayStation home menu.

## Limitations

- No AMD GCN command processor, GNM, GNMX, PS4 shaders, graphics driver, game rendering, or genuine home menu.
- No performance optimization, texture loading, or command submission from guest executable code.
- This tests the CPU/GPU architecture boundary without depending on copyrighted platform binaries.
- Existing unsigned IPA workflow is unchanged. Full iOS compilation needs GitHub Actions on macOS; local native C++ tests do not validate Apple Metal itself.
