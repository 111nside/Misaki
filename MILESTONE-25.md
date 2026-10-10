# Misaki Milestone 25 — Bounded AMD GCN Shader ISA & Buffer Resource Metadata

## Purpose

Start interpreting authentic AMD GCN instruction *encodings* using an original, self-authored
shader fixture. This deliberately small read-only analyzer connects the Milestone 24
PM4 draw-state pixel shader address to a guest-memory shader program and parses a
128-bit GCN3-style buffer-resource descriptor.

It is **not** GCN shader execution, wavefront simulation, PS4 GNM/GNMX support,
shader translation to Metal, PS4 SELF/PRX/firmware loading or a home-menu boot.

## What's implemented

- Supported exact 32-bit ISA encoding families: SOPK, SOP1, SOP2, SOPP, VOP1, VOP2.
- Supported opcodes: s_movk_i32, s_mov_b32, s_not_b32, s_add_u32,
  s_sub_u32, s_and_b32, s_nop, s_endpgm, v_mov_b32, v_add_f32, v_mul_f32.
- Tracks instruction address, DWORD offset/length, opcode, SGPR/VGPR selectors,
  inline immediate fields, and optional trailing 32-bit literal constants.
- Requires a complete fixed-size shader trace ending in S_ENDPGM; explicit
  rejection of unknown opcodes/encodings, unaligned addresses, missing literals,
  unsupported control flow, special registers, and trailing data.
- Verifies read permissions when copying shader and descriptor DWORDs from
  Misaki's isolated guest virtual memory.
- Decodes raw buffer base (48-bit), 14-bit stride, record count, channel selects,
  data/numeric format and swizzle flags; marks only a restricted subset as
  internally consistent *linear metadata*, not as GPU accessible.
- Links to the Milestone 24 self-authored PM4 draw fixture, with the test shader
  mapped read-only at its decoded 0x100000 pixel-shader address.
- Uses a bounded C ABI with transactional capacity checks and an iOS GCN ISA tab.
- Leaves earlier graphics and PM4 diagnostic tabs untouched.

## Expected diagnostic on iPhone

1. Open **GCN ISA** (possibly under **More**).
2. Tap **Decode guest GCN pixel shader**.
3. Expect **GCN shader decode: PASS**:

```
PM4 packets/draws=11/1
Shader instructions=8
Scalar/Vector=5/3
Literal DWORDs=1
Code DWORDs=9
S_ENDPGM=true
Shader/Descriptor read-only=true/true
Status=0
```

Expected pixel shader address: `0x100000`; buffer base: `0x400000`,
stride: `16 bytes`, records: `64`, raw data format: `14`.
The eight instruction names shown should be:

```
s_movk_i32
s_mov_b32
s_add_u32
v_mov_b32
v_add_f32
v_mul_f32
s_nop
s_endpgm
```

## Build / tests

```
cmake -S MisakiNative -B build/native -DCMAKE_BUILD_TYPE=Debug
cmake --build build/native --parallel 2
ctest --test-dir build/native --output-on-failure
```

The native suite has 16 tests total; the new GCN ISA suite has 704
assertions covering valid and invalid streams, bounded randomized inputs,
C ABI output bounds, guest read permissions and descriptor interpretations.
Locally verified on GCC, and Clang with ASan/UBSan. The new Swift interface
was type-checked using `MisakiBridgeAll.h`. The complete Xcode/iPhone build
must be verified in GitHub Actions.

## References and limitations

Encodings follow the AMD GCN scalar and vector instruction format tables;
the buffer-resource fields follow the GCN3 resource descriptor layout.
A PS4-era variant may differ in exact field meanings and its shader binary
container; this does not claim a validated PS4 hardware profile.

- AMD Southern Islands instruction set: https://www.amd.com/content/dam/amd/en/documents/radeon-tech-docs/instruction-set-architectures/southern-islands-instruction-set-architecture.pdf
- AMD GCN3 instruction set: https://www.amd.com/content/dam/amd/en/documents/radeon-tech-docs/instruction-set-architectures/gcn3-instruction-set-architecture.pdf

No AMD, Sony, game or firmware source was copied into the project. All guest
fixtures were written for Misaki for testing only.
