# Misaki Milestone 24 — Draw-time PM4 register state

## What changed

The native PM4 investigation now replays register writes from Milestone 23's recursively decoded indirect buffers and records a **per-draw snapshot** when an actual `DRAW_INDEX_AUTO` packet occurs. Register updates after a draw do not retroactively change its state.

The bounded metadata interpreter captures:

- CB_COLOR0_BASE candidate color address (raw register is in 256-byte units)
- Pixel-shader program LO/HI candidate address (256-byte units; restricted high-register layout)
- Scissor origin and extent from the tracked context registers
- Raw CB_COLOR0_PITCH tile-max field, CB_COLOR0_INFO format field, CB_TARGET_MASK and observed-register mask
- Guest packet address, nesting depth, packet sequence index and DRAW_INDEX_AUTO vertex count

It detects missing registers, disabled/multiple color targets, invalid scissor rectangles, impossible shader/target addresses, and unspecified format fields. The diagnostics preserve a distinction between **metadata that appears internally consistent** and actual GPU resources: `metadataReady` is *not* a statement that the guest buffer is renderable or that a PS4 shader is executable.

A small C bridge and a new `PM4 State` iOS tab report the first synthetic diagnostic. The other 14 native test suites and all previous Metal / PM4 views remain unchanged.

## On-device verification

Open **PM4 State** (possibly under **More**) and select **Analyze PM4 draw state**.

Expected:

```
GCN draw-state analysis: PASS
Packets=11
Indirect buffers=2
Register writes=8
Draws=1
Consistent/Rejected=1/0
Status=0
```

The displayed draw should show:

```
Vertex count=3
Guest packet=0x5002C
Packet/depth=9 / 1
Color target candidate=0x20000000
Pixel shader candidate=0x100000
Scissor origin=(10, 10)
Scissor size=190 × 110
Pitch tile max (raw)=127
Color format field=0x1B
Target mask=0xF
Metadata internally consistent=Yes
```

## Actual PS4 compatibility status

This is a read-only state interpreter for a limited subset of AMD GCN-era **PM4 packet metadata**. It does not decode full PS4 render-target tiling/format information, upload GPU virtual memory, translate AMD shader ISA to Metal, implement GNM/GNMX, execute drawing, or boot PS4 firmware. No actual Sony firmware or game data is bundled, and the test fixture is self-authored. The raw register values used for the two pointer candidates are **not** checked against a guest GPU address-space mapping.

## Checks performed locally

- All **15** CMake/CTest native suites passed (14 existing suites + 1 new).
- **107** native assertions passed in the new draw-state suite, including two draws with different scissor state, malformed input rejection, inaccessible memory, and C ABI transactional output.
- All **15** suites also passed with Clang AddressSanitizer and UndefinedBehaviorSanitizer.
- New Swift C-bridge code type-checked against the actual C header; new SwiftUI files passed Swift syntax parsing.
- Full iOS application compilation must still be verified with your GitHub Actions macOS runner.

## Installation

Put `Misaki-Milestone24-PM4-Draw-State.zip` in Downloads, then:

```powershell
cd "$HOME\Downloads\Misaki"
git pull --ff-only
Expand-Archive -Path "$HOME\Downloads\Misaki-Milestone24-PM4-Draw-State.zip" -DestinationPath . -Force
git add .
git commit -m "Add AMD PM4 draw-time state interpretation"
git push origin main
```

Verify **Test Misaki Native Core** and **Build Misaki iOS** on GitHub Actions, then run the new diagnostic on your iPhone.
