# Misaki Milestone 30 — interleaved GCN ALU + MUBUF, one Metal kernel

## What this adds

- A **self-authored**, real-format **GCN1.1** shader stream with 11 ordered instructions (13 DWORDs), integrating scalar ALU, VGPR moves, `BUFFER_LOAD_DWORD`, `BUFFER_STORE_DWORD`, and `S_ENDPGM`.
- Reuses Milestones 25/26's GCN ISA parser + typed ALU lowering and Milestone 29's exact two-DWORD MUBUF parser, rejecting unknown encodings, flags, and register layouts.
- A bounded **one-lane CPU reference** that uses read-protected shader/descriptors/input memory and writes only one valid output record.
- **One generated Metal compute kernel** (no separate ALU preprocessing kernel) reproducing the mixed instruction order. The iPhone runner compiles and dispatches this source, reads back eight registers plus 64 output records, and only reports `PASS` when the output exactly matches the independent C++ reference.
- New **GCN Fused** tab under **More**. Existing graphics, PM4, GCN ISA/IR/Metal/MUBUF and other diagnostics are retained.

## Verification

- `cmake -S MisakiNative -B build/native -DCMAKE_BUILD_TYPE=Debug`
- `cmake --build build/native --parallel 4`
- `ctest --test-dir build/native --output-on-failure`
- Linux local result: **21/21 native suites PASS** including **1,804 new milestone-30 assertions**.
- Clang 17 with `-fsanitize=address,undefined` and leak checks: **21/21 PASS**.
- New Swift sources pass Swift syntax checking. Full XcodeGen/Xcodebuild/Metal compilation requires GitHub Actions/macOS; it has **not** been verified by this package's local Linux build.

## Expected in-app diagnostic

Open **GCN Fused** (possibly under **More**) and press **Execute fused GCN shader on Metal**:

```
GCN fused shader decode: PASS
Instructions=11
Code DWORDs=13
ALU/Buffer loads/Buffer stores=8/1/1
CPU executed=11
Record index=7
Loaded/Stored=31/31
Guest code/descriptor read-only=true/true
Input read-only / output writable=true/true
Native status=0

Metal fused shader comparison: PASS
GPU status=0
Output record 7=31
```

Expected GPU register readback `s1=7, s2=7, s3=14, s10=25, s12=18, v4=7, v8=31, v9=31`. All 64 output DWORDs match C++; only record 7 changes.

## Deploy on Windows PowerShell

Download the ZIP into `Downloads`, then:

```powershell
cd "$HOME\Downloads\Misaki"
git pull --ff-only
Expand-Archive -Path "$HOME\Downloads\Misaki-Milestone30-Fused-GCN-Metal.zip" -DestinationPath . -Force
git add .
git commit -m "Add fused GCN shader execution in a single Metal kernel"
git push origin main
```

Check GitHub Actions: **Test Misaki Native Core** and **Build Misaki iOS**. Test the result on your iPhone.

## Explicit limitations

- A deliberately strict one-lane, one-work-item diagnostic: no 64-lane wavefronts, ISA-wide control flow, data hazards, scalar/vector memory modes, cache coherence, PS4 GNM/GNMX calls, Sony binaries or actual firmware boot.
- The diagnostic shader is self-authored and uses GCN instruction **formats**, not a copied PS4 program. It is not a PlayStation 4 home-menu boot test.
- Completing this milestone ends the **original 30-step research roadmap**, **not** PS4 emulator development; major OS, kernel, GCN GPU and graphics API work remains.
