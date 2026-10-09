# Misaki Milestone 14: Isolated Guest Processes + Virtual Filesystem

This milestone adds a bounded **research-only** process manager and read-only,
**in-memory** virtual filesystem to Misaki's portable C++ CPU engine. It does
not emulate Sony's Orbis kernel, process APIs, filesystem, firmware, or GPU.

## What's implemented

- `GuestVirtualFileSystem` stores up to 32 small, self-authored files in memory;
  **guest paths are not host paths** and cannot escape through `..`, `.` or `//`.
- `GuestProcessServices` provides independent guest file descriptor tables,
  guest process IDs, and a test-specific file-service interface.
- New toy services: `open` (`0x200`), `read` (`0x201`), `close` (`0x202`),
  `seek` (`0x203`), and `processID` (`0x204`). **These IDs are not PS4 or
  FreeBSD syscalls.** Only read-only opens are supported.
- Guest `read` copies into sandboxed guest memory with a 4 KiB operation cap;
  a bad pointer does not partially write data or advance the guest file cursor.
- `GuestProcessManager` runs at most four isolated address spaces under a
  deterministic, bounded cooperative scheduler. This is not kernel threading.
- A self-authored ELF64 application reads `/app/greeting.txt`, outputs its first
  five bytes, yields, closes its descriptor, and reports its simulated PID.
  Two guest processes run independently and both produce `Hello`.
- A stable C API, Swift wrapper, Core-tab diagnostic and additional XCTest.
- 121 new native C++ assertions plus all prior native test suites.

## Expected result on iPhone

Open **Core -> Execute virtual filesystem and processes**:

```text
Guest VFS and processes: PASS
Processes=2
Instructions=44
Open/Read/Close=2/2/2
Yield events=2
Guest output=HelloHello
PID1=1001, PID2=1002
Stacks restored=true
Status=0
```

## Build and test

From the repo root:

```bash
cmake -S MisakiNative -B build/native -DCMAKE_BUILD_TYPE=Debug
cmake --build build/native --parallel 2
ctest --test-dir build/native --output-on-failure
```

`Test Misaki Native Core` runs the new CTest target automatically. The existing
`Build Misaki iOS` workflow still performs simulator tests, device build and
unsigned IPA packaging. The full Xcode build must run on GitHub Actions or a Mac.

## Boundaries

No `.pkg` files, firmware, system keys, PS4 SELF/PRX files or Sony code are
required or included. No guest file operation reads/writes actual iPhone files.
This is a useful *architecture* exercise, not evidence that PS4 software boots.
Real Orbis-compatible process management, executable services and GCN/Metal
translation remain significant independent engineering projects.
