# Misaki — Milestone 29: AMD GCN1.1 MUBUF → Metal buffer access

Milestone 29 advances the working Milestone 28 resource experiment from a
*synthetic resource IR* to a **strictly bounded decoder for actual 64-bit
GCN1.0/1.1 untyped memory-buffer instruction encodings**. It is not a complete
PS4 shader translator or an original PS4 home menu.

## New native functionality

- Parses authentic GCN MUBUF instruction bitfields: 6-bit encoding `111000`,
  7-bit opcode, byte offset, `OFFEN`, `IDXEN`, `GLC`, `ADDR64`, `LDS`, `SLC`,
  `TFE`, `VADDR`, `VDATA`, `SRSRC` (4-SGPR-aligned), `SOFFSET`.
- Supports decoding **BUFFER_LOAD_DWORD (0x0C)** and
  **BUFFER_STORE_DWORD (0x1C)** for the GCN1.0/1.1 encoding profile. Rejects
  unrecognized opcodes, reserved bits, half instructions and oversized input.
- Decodes the instructions from read-only, permission-checked guest memory.
- Evaluates a specifically supported two-instruction sequence in isolated
  guest memory: `BUFFER_LOAD_DWORD v8, v4, s[0:3], 0 idxen`, then
  `BUFFER_STORE_DWORD v8, v4, s[4:7], 0 idxen`.
- Reads separately protected 128-bit GCN resource descriptors for input and
  output. The input contains 64 x 16-byte records at `0x400000`; the output
  contains 64 x 4-byte records at `0x500000`.
- Metal code generation uses the **decoded operands**, resource strides and
  record counts, rejecting unsupported modes. The generated MSL is limited
  to one GPU thread, index read/write, and unsigned DWORD semantics.
- Reuses Milestone 27's actual Metal ALU kernel as the first dispatch and
  Milestone 28's tested two-kernel runner as the second. The generated
  M29 kernel is `misaki_mubuf29`. The first ALU result (`7`) is deliberately
  staged into guest `v4`, making the lane/index association explicit.
- Every result is checked against an *independent* C++ reference. Only
  record 7 changes, from its sentinel to `31`. The other 63 output records
  remain identical, and all six ALU result registers must match.

## iPhone diagnostic

Open **GCN MUBUF** (possibly under **More**) and tap
**Decode GCN MUBUF and compare Metal**.

Expected native report:

```
GCN MUBUF decode: PASS
Instructions=2
Opcodes LOAD/STORE=12/28
VADDR/VDATA=v4/v8
Resource SGPRs=s0, s4
Record index=7
Loaded/Stored=31/31
Guest code/descriptor read-only=true/true
Source read-only / output writable=true/true
C++ status=0
```

After on-device Metal execution, expected:

```
Metal MUBUF comparison: PASS
GPU status=0
Output record 7=31
```

This does **not** produce a new moving image. It tests actual GCN-format
buffer-memory opcodes on a carefully limited single-lane diagnostic.

## Test results

- Full rebuilt native project: **20/20 CTest suites passed** with GCC.
- Full native project: **20/20 passed** with Clang + ASan/UBSan.
- **965 new assertions** in `GuestMUBUFTests.cpp`, covering instruction
  bitfields, opcode allowlisting, guest address/permission faults, bad
  descriptors, read-only output, wrong addressing flags, output integrity,
  deterministic hashes, no-partial-write C ABI and decoder fuzz tests.
- Swift source syntax checks and Swift-to-C bridging typechecks passed
  locally. **Full iOS compilation and Metal execution require GitHub Actions
  and an actual iPhone.**
- The existing M18–M28 tests, views, and original M28 kernel remain available.

## Apply the overlay

```powershell
cd "$HOME\Downloads\Misaki"
git pull --ff-only
Expand-Archive -Path "$HOME\Downloads\Misaki-Milestone29-GCN-MUBUF.zip" -DestinationPath . -Force
git add .
git commit -m "Decode GCN MUBUF memory instructions into Metal"
git push origin main
```

Check **Test Misaki Native Core** and **Build Misaki iOS** GitHub Actions.

## Limitations and source references

This is strictly **GCN 1.0/1.1 MUBUF** encoding for two DWORD operations, not
GCN ISA-wide coverage, packed/typed formats, texture sampling, coherency,
atomics, MUBUF OFFEN/ADDR64 execution, VGPR/SGPR wavefront scheduling, PS4
kernel services, Sony GNM/GNMX libraries, games or firmware. The guest
program and buffers are authored for the diagnostic; no copyrighted console
firmware is supplied.

Bit layouts and opcode values are cross-checked against AMD's public
*Southern Islands Instruction Set Architecture* and the independent
CLRX GCN MUBUF instruction reference:

- https://www.amd.com/content/dam/amd/en/documents/radeon-tech-docs/instruction-set-architectures/southern-islands-instruction-set-architecture.pdf
- https://github.com/CLRX/CLRX-mirror/wiki/GcnInstrsMubuf
