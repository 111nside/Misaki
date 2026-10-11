# Misaki mega update — Milestones 41–45

## Scope (honest compatibility boundaries)

This is a **synthetic integration milestone**, not a working PS4 kernel,
SELF decryptor, Sony library HLE implementation, or PlayStation home menu boot.
Every executable, NID-like identifier, syscall number, and GPU test image is
independently authored for diagnostic purposes; no PS4 firmware or keys included.

### Five separate test targets

- **41 / SCE ELF stage + relocations** — recognize self-authored unwrapped
  SCE_DYNEXEC ELF64; validate PT_LOAD file/guest ranges, reject W+X and
  overlapping mappings; apply explicit, fixture-provided *relative* relocations
  before sealing segment permissions. A genuine encrypted or compressed SELF is
  rejected, not decrypted or executed. Does not yet parse Sony relocation tables.
- **42 / import binding** — exact SCE-shaped synthetic module/library/NID/version
  lookup, transactional guest import-slot patching, and read-only GOT sealing;
  unresolved or ambiguous requests fail without partial guest changes.
- **43 / guest service ABI** — controlled process ID, deterministic clock,
  bounded output writes, and process exit through the existing x86-64 interpreter.
  IDs `0x741–0x744` are Misaki diagnostic IDs, **not actual PS4 kernel syscall
  numbers**; no host file, network, or system access.
- **44 / GPU memory** — checks a recursively decoded PM4 draw state's target
  address and shader execution permission; submits an independently authored
  Misaki 2D command queue, waits for its fence, and transactionally copies a
  software-rasterized RGBA image into mapped guest color-target memory.
  **Does not execute PM4 draws or native GCN shaders.**
- **45 / initialization pipeline** — combines stages 41–44 in one test guest
  session, executes the imported RX function (RAX=42), reports the completed
  framebuffer fence, and confirms that real firmware boot is **blocked** by
  five unimplemented prerequisites.

## Installation

Unzip into the local checkout of `111nside/Misaki`:

```powershell
cd "$HOME\Downloads\Misaki"
git pull --ff-only
Expand-Archive -Path "$HOME\Downloads\Misaki-Milestones41-45-SCE-Integration.zip" -DestinationPath . -Force
git add .
git commit -m "Integrate SCE staging services and GPU memory milestones 41-45"
git push origin main
```

### Verification

- GitHub Actions **Test Misaki Native Core** and **Build Misaki iOS** should pass.
- iPhone: **SCE Runtime** (possibly under **More**) → **Execute SCE compatibility
  integration test**.
- Expected: 41–45 five diagnostic PASS indicators; segments=1, relocations=1,
  imports=2, service calls=3, output bytes=5, RAX=42, PM4 packets=11,
  Misaki-2D packets=10, sprites=2, fence=1, blockers=5, firmware attempted=false,
  home menu booted=false, status=0.

Run native suites locally:

```bash
cmake -S MisakiNative -B build/native -DCMAKE_BUILD_TYPE=Debug
cmake --build build/native --parallel 2
ctest --test-dir build/native --output-on-failure
```

CTest suites are `compat41-integrated-stage`, `compat42-integrated-stage`,
`compat43-integrated-stage`, `compat44-integrated-stage`, and
`compat45-integrated-stage` in addition to all previous suites. The complete
native suite passed (36/36) under GCC and with Clang ASan/UBSan before release.
The full Xcode and actual device run must still be confirmed by GitHub Actions
and a signed install on the user's iPhone.
