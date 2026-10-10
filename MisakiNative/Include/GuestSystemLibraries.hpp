#pragma once

#include "GuestDynamicExecution.hpp"
#include "GuestProcesses.hpp"

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace misaki {

// First integrated userspace-library prototype. No PS4/FreeBSD ABI,
// proprietary NIDs, host filesystem access, or Sony firmware is involved.
// An ordinary, independently authored ET_DYN guest calls a separately
// mapped library. Its x86-64 code reaches the per-process VFS via SYSCALL.
struct GuestSystemLibraryReport {
    X64ExecutionResult execution;
    DynamicLinkSummary linked;
    std::uint32_t processID = 0;
    std::uint32_t opens = 0;
    std::uint32_t reads = 0;
    std::uint32_t closes = 0;
    std::uint32_t writes = 0;
    std::uint32_t yields = 0;
    std::uint64_t importAddress = 0;
    bool importReadOnly = false;
    bool libraryReadOnly = false;
    std::string output;
};

// Unlike a C++ callback pretending to be a guest function, the exported
// library routine consists of x86-64 instructions executed by the backend.
// It opens /system/message.txt, reads and echoes five bytes, closes its fd,
// reads a toy page-size service, and returns RAX=42.
std::vector<std::uint8_t> makeGuestSystemLibraryCode();

// Runs the self-authored module through ET_DYN loading, dynamic symbol
// resolution, guest-memory protection, x86-64 CALL/RET, and VFS services.
// Returns nullopt for setup/link errors, not for guest execution faults.
std::optional<GuestSystemLibraryReport> runGuestSystemLibraryDiagnostic();

} // namespace misaki
