# Misaki Milestone 27 — Restricted GCN IR → Metal compute comparison

## What actually works

- Starts with the same self-authored AMD-format GCN instruction fixture, guest-memory protection, PM4 draw-state, and typed 8-instruction IR as Milestone 26.
- Emits a real Metal Shading Language (MSL) **compute kernel** from a strict subset of 11 IR operations: scalar moves, NOT, add/sub/AND; vector bitwise moves and float add/multiply; NOP and END.
- On iOS, asks Metal to **compile** this generated source, runs a single compute work-item, reads back six 32-bit registers, and requires an exact match with the independent C++ IR evaluator before reporting GPU PASS.
- Exposes the exact generated MSL source and per-register CPU/GPU comparisons on the new **GCN Metal** tab.
- All existing PM4, shader, CPU, and Metal graphics demonstrations are preserved.

## Diagnostic

Tap **GCN Metal** (under **More** if tabs overflow), then **Compile and execute translated Metal shader**. The first native section should show:

```
IR → MSL translation: PASS
IR operations=8
Emitted ALU statements=6
```

On a compatible iPhone the second section should show `Metal GPU comparison: PASS`, with these expected raw values:

| Register | Decimal / float | Raw bits |
|---|---|---|
| s1 | 7 | 0x00000007 |
| s2 | 7 | 0x00000007 |
| s3 | 14 | 0x0000000E |
| v0 | 2.0f | 0x40000000 |
| v1 | 4.0f | 0x40800000 |
| v2 | 4.0f | 0x40800000 |

If compilation, GPU execution, or comparison fails, the UI reports FAIL with an error or mismatched bits. Source generation alone does not count as Metal execution success.

## Tests / limitations

- 18 C++ test suites passed using GCC and Clang plus AddressSanitizer/UndefinedBehaviorSanitizer.
- 443 dedicated native tests assert source-generation, output bit patterns, alternative operations, failure cases, and bounded buffers.
- Swift bridge type-checked against the native header on Linux; Swift files were syntax-checked. Full Metal/iOS compilation **was not available locally**; verify with the `Build Misaki iOS` GitHub workflow and the actual device.
- Metal is a single-thread compute demo, not graphics rasterization. There is no GCN wavefront semantics, shader resource fetching, general game shader support, firmware support, or PS4 system-menu booting.

## Apply ZIP to repo

```powershell
cd "$HOME\Downloads\Misaki"
git pull --ff-only
Expand-Archive -Path "$HOME\Downloads\Misaki-Milestone27-GCN-to-Metal.zip" -DestinationPath . -Force
git add .
git commit -m "Add restricted GCN IR to Metal compute translation"
git push origin main
```

Monitor GitHub Actions for both `Test Misaki Native Core` and `Build Misaki iOS`. The iOS workflow may skip the on-simulator GPU execution test if the runner lacks a Metal device; physical iPhone testing remains essential.
