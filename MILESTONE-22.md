# Milestone 22 — AMD GCN-era PM4 packet parsing

This milestone shifts attention from Misaki's original colored-square rendering protocol toward documenting and interpreting actual AMD GPU command *formats*. The code is an original, conservative PM4 **decoder**, not a real PS4 GPU backend.

## Features

- Parses standard Type-0, Type-2, and Type-3 PM4 packet headers, including Type-3 count and opcode fields.
- Captures ordinary Type-0 and selected Type-3 register writes, including CONFIG, CONTEXT, SH, and UCONFIG register banks.
- Records (but **does not execute**) `DRAW_INDEX_AUTO` and `EVENT_WRITE` packet metadata.
- Recognizes NOP, INDEX_TYPE, NUM_INSTANCES, and common register-write packets.
- Copies the command buffer from a read-only `GuestMemory` region with bounded, permission-checked reads.
- Rejects truncated command buffers, unsupported opcodes, predicated packets, indirect buffers, out-of-range registers, and exhausted output capacity without partially exporting a trace.
- Keeps all earlier Metal demonstrations, CPU tests, and IPA packaging unchanged.

## Known limitations

- The trace is a deliberately self-authored sample containing documented AMD packet encodings. It is **not** a capture from Sony firmware, software or hardware.
- No implementation of GNM/GNMX, the PS4's Liverpool GPU context, indirect buffer recursion, shaders, geometry setup, GPU memory, display scanout, or hardware execution.
- The decoded register addresses are DWORD-based architectural indices, not a claim of full GPU register behavior.
- No attempt to load PS4 games or encrypted firmware.

## Tests

Native independent test target: `misaki_guest_pm4_tests`. The GitHub native-core workflow includes it automatically through CTest.

On iOS, select **PM4 → Decode guest PM4 command stream**. Expected outcome: nine packet headers, four register writes, one draw event with three vertices, a nonzero checksum, and status zero.

## References

- AMD, *Southern Islands Series Instruction Set Architecture / Programming Guide*, PM4 packet definitions: https://www.amd.com/content/dam/amd/en/documents/radeon-tech-docs/programmer-references/si_programming_guide_v2.pdf
- Public PM4 command structures and opcode map in shadPS4 (referenced for interoperability facts; source is not copied): https://github.com/shadps4-emu/shadPS4/tree/main/src/video_core/amdgpu

This milestone does not add shadPS4 code or pull in its GPL-licensed implementation.
