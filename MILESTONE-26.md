# Misaki Milestone 26 — GCN Shader Intermediate Representation

## What this milestone does

Builds a **strict, tiny, single-lane** software shader IR and diagnostic evaluator
on top of Milestone 25's real-format, self-authored AMD GCN instruction fixture.
Nothing from PS4 firmware, system files or commercial software is required.

The path tested here is:

1. Decode a real-form, self-authored AMD PM4 nested command stream from guest memory.
2. Identify the candidate pixel-shader guest address at the draw (M24).
3. Read and decode self-authored GCN ISA instructions in read-only guest memory (M25).
4. Lower those instructions into a typed intermediate representation (M26).
5. Evaluate that IR in a **one-lane** software scalar/vector ALU diagnostic.
6. Report final 32-bit integer and float register bits through a bounded C ABI.

## IR subset

- `s_movk_i32`, `s_mov_b32`, `s_not_b32`, `s_add_u32`, `s_sub_u32`,
  `s_and_b32`, `s_nop`, `s_endpgm`.
- `v_mov_b32`, `v_add_f32`, `v_mul_f32`.
- SGPR indices 0..101, VGPR indices 0..255, scalar inline 0..64 and -1..-16,
  an optional 32-bit literal, sign-extended 16-bit SOPK immediates.
- 32-bit modulo arithmetic, bitwise operations and basic finite IEEE754 float operations.
- Strict instruction sequence/size validation; read-only guest input and explicit
  rejection of unsupported instructions and invalid operands.

## iPhone diagnostic

Open **GCN IR** (possibly in More), press
**Translate and evaluate guest shader**. Expected:

```
GCN shader IR: PASS
PM4 packets/draws=11/1
IR instructions=8
Scalar/Vector=5/3
Executed=8
Scalar/Vector writes=3/3
S_ENDPGM=true
Source/Descriptor read-only=true/true
Status=0
```

Expected scalar register values: `s1=7`, `s2=7`, `s3=14`.
Expected floating vector register values: `v0=2.0`, `v1=4.0`, `v2=4.0`.
The UI displays the lowered IR instructions with typed operands and three
independent checksums (source, IR, result).

## Testing

- The native build provides 17 test suites; **all 17 passed locally** with GCC
  and with Clang/AddressSanitizer/UndefinedBehaviorSanitizer.
- The new `native-gcn26-shader-ir-tests` suite contains **576 assertions**,
  including deterministic fuzz smoke checks, invalid trace rejection,
  float NaN/Infinity rejection, literal handling, and C ABI atomicity.
- Swift API typechecks against `MisakiBridgeAll.h` on Linux. New UI and unit
  test files pass Swift syntax parsing.
- **The macOS/iOS device build and iPhone test are NOT verified locally.**
  Verify `Build Misaki iOS` and `Test Misaki Native Core` in GitHub Actions.

## Scope, accuracy, and limitations

A typed shader IR and one-lane diagnostic interpreter are **not** an AMD GCN
wavefront interpreter, Sony shader binary parser, shader resource/sampler
implementation, graphics rasterizer, Metal source-code generator, or real PS4
GPU driver. The isolated software evaluator is deliberately more restrictive
than actual GCN hardware (for example, rejects floating nonfinite values).
This milestone does not bring Misaki close to booting original PS4 firmware
on its own; substantial system and GPU work remains.

## Install

Unzip as an overlay to the root of `111nside/Misaki` on branch `main`, then
commit and push. Files not present in the overlay remain untouched.

```powershell
cd "$HOME\Downloads\Misaki"
git pull --ff-only
Expand-Archive -Path "$HOME\Downloads\Misaki-Milestone26-GCN-Shader-IR.zip" -DestinationPath . -Force
git add .
git commit -m "Add typed GCN shader IR and single-lane evaluator"
git push origin main
```
