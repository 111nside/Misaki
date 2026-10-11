# Misaki Milestone 28 — GCN IR → Metal resource-memory execution

## What works

- Preserves all earlier emulator, PM4 and graphics diagnostics.
- Consumes M25's original-format AMD GCN buffer descriptor metadata (base `0x400000`, 64 records, 16-byte stride); it verifies guest-memory permissions and byte bounds.
- Reuses M27's real GCN-IR-to-Metal ALU kernel **without modification**. That first compute dispatch produces six scalar/vector results **on the GPU**.
- Generates a second Metal compute kernel from an explicitly typed, **synthetic** resource-stage IR: `loadU32 -> addRegister -> storeU32`. The second kernel reads the GPU register buffer from the first dispatch, accesses a copied 1024-byte guest input buffer, and writes an output buffer.
- Executes both kernels in order on one Metal command buffer. Reads back all six ALU registers and all 64 output records. Reports PASS **only if both kernels execute and every word exactly matches the independent C++ software reference**.
- The software reference fetches the first DWORD of a 16-byte record from a read-protected `GuestMemory` mapping and verifies the isolated writable guest-output mapping.
- Strict index bounds, descriptor checks, MSL size limits, and C ABI capacity checks (no partial output copies).

## How to run

Open **GCN Resources** (possibly under **More**) and tap **Run GPU resource-memory comparison**.

Native diagnostic expected:

```
Resource IR: PASS
Operations=3
Buffer address=0x400000
Records / stride=64 / 16 bytes
GPU read/write record=7/6
Loaded + scalar = 31 + 14 = 45
Protected input / writable output=true/true
C++ status=0
```

On your iPhone, **Metal resource comparison: PASS** means both kernels executed and all reference values match. Output record 6 should become `45`, while the other 63 remain untouched; `GPU status=0`.

If the host simulator has no Metal device, the GPU-specific XCTest is skipped. A device is required for the full execution comparison.

## Tests and scope

- 19/19 native CTest suites passed with GCC and Clang+ASan/UBSan.
- Milestone 28 has 695 new native assertions covering guest read permissions, register-derived indices, underflow/overflow, descriptor rejection, source translation, deterministic hashes, and no partial C ABI writes.
- New Swift API type-checked with the **full C bridging header** on Linux; Swift source parsed. Actual Metal/iOS compilation and two-kernel execution **must be verified on your Mac GitHub runner and iPhone**.
- **Not implemented:** Real GCN MUBUF/MTBUF instruction decode, wavefront execution, real PS4 resource formatting, graphics shaders/rasterization, Sony firmware, or home menu. The memory stage uses a self-authored 3-op IR and interprets the first DWORD as uint32 under its own convention. It is NOT proof that a commercial PS4 shader can run.

## Apply ZIP

```powershell
cd "$HOME\Downloads\Misaki"
git pull --ff-only
Expand-Archive -Path "$HOME\Downloads\Misaki-Milestone28-Metal-Resources.zip" -DestinationPath . -Force
git add .
git commit -m "Add GCN IR Metal buffer resource execution"
git push origin main
```

Verify `Test Misaki Native Core` and `Build Misaki iOS` workflows, then use the on-device comparison.
