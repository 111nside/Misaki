# Misaki Milestone 23 — Nested AMD PM4 Command Buffers

## Result

This milestone extends the real-form AMD GCN PM4 packet-header decoder with a **read-only, bounded** traversal of `INDIRECT_BUFFER` (0x3F) and `INDIRECT_BUFFER_CONST` (0x33) packets, raw render-target/scissor/shader register observation, an iOS diagnostic, and regression tests.

The example PM4 commands and register contents are **self-authored synthetic fixtures**, not Sony firmware or game captures.

## Test on iPhone

Open **PM4+** (or find it under the tab bar's **More** menu), and tap **Decode nested PM4 buffers**.

Expected diagnostic:

```
Nested GCN PM4: PASS
Packets=11
Indirect buffers=2
Const buffers=1
Max depth=2
Guest DWORDs=34
Register writes=8
Draw/Event=1/1
Render-target metadata=true
Status=0
```

Under **Observed GPU register values** it should show `CB_COLOR0_BASE=0x200000`, `CB_COLOR0_PITCH=0x7F`, `CB_COLOR0_INFO=0x1B`, `CB_TARGET_MASK=0xF`, and the scissor and shader register values. The **Decoded execution order** section includes the root, child, and grandchild packet addresses and depth.

## Implementation boundaries

- Guest buffers are copied only through `GuestMemory::read8` (guest read permissions enforced).
- Type-3 PM4 packet header layout, indirect packets, and known register packet decoding are grounded in AMD GCN-era PM4 framing. Unknown packets are rejected rather than emulated.
- The decoder enforces an overall packet budget, DWORD budget, register-write budget, alignment, read-permission checks, maximum nesting, and cycle detection.
- It rejects predicated indirect packets and unsupported chained/advanced IB control flags.
- No GPU commands are executed. Shader pointers, color bases, and other register values are preserved **as raw metadata**; they are not translated, dereferenced, or validated as actual render targets.
- No PS4 SELF/PRX/firmware, Sony system services, or actual AMD GPU shader ISA execution are included.
- Previous PM4 and Metal graphics demonstrations remain unchanged.

## Local checks

- CMake/CTest: all 14 native test suites passed.
- PM4 decoder test: 673 assertions passed unchanged.
- New nested PM4 test: 335 assertions passed.
- Clang AddressSanitizer + UndefinedBehaviorSanitizer: all 14 native suites passed.
- Swift API typechecked against the actual C headers using a Clang module; new SwiftUI files were syntax-parsed.

A full iOS app build requires the macOS GitHub Actions runner.

## Install

Save `Misaki-Milestone23-Indirect-PM4.zip` in Downloads, and from PowerShell:

```powershell
cd "$HOME\Downloads\Misaki"
git pull --ff-only
Expand-Archive -Path "$HOME\Downloads\Misaki-Milestone23-Indirect-PM4.zip" -DestinationPath . -Force
git add .
git commit -m "Add nested AMD PM4 decoding and raw GPU state tracking"
git push origin main
```

Then verify `Test Misaki Native Core` and `Build Misaki iOS` in GitHub Actions. The existing unsigned IPA workflow is untouched.
