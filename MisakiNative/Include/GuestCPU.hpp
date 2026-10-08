#pragma once

#include "MisakiCore.hpp"

namespace misaki {

// Intentionally bounded, non-JIT x86-64 test executor. NOT a PS4 CPU emulator.
enum class GuestStop : std::uint8_t {
    halted,
    invalidOpcode,
    memoryFault,
    stackFault,
    stepLimit
};

struct GuestLoadedImage {
    std::uint64_t entry = 0;
    std::uint32_t segments = 0;
};

// Maps the PT_LOAD segments from a small x86-64 ELF64 image. Mappings are
// writable only during segment initialization, then protected per ELF p_flags.
// For ET_DYN, loadBias is added to p_vaddr and e_entry.
std::optional<GuestLoadedImage> loadGuestELF(const std::uint8_t *bytes,
                                             std::size_t length,
                                             std::uint64_t loadBias,
                                             GuestMemory &memory);

struct GuestCPUResult {
    GuestStop stop = GuestStop::invalidOpcode;
    std::uint64_t rax = 0;
    std::uint64_t rip = 0;
    std::uint64_t rsp = 0;
    std::uint32_t instructions = 0;
    bool zeroFlag = false;
    bool stackRestored = false;
};

class GuestCPU {
public:
    explicit GuestCPU(GuestMemory memory, std::uint64_t entry);
    GuestCPUResult run(std::uint32_t maxSteps = 1000);

private:
    GuestMemory memory_;
    std::uint64_t rip_ = 0;
    std::uint64_t rax_ = 0;
    std::uint64_t rsp_ = 0;
    std::uint64_t initialSP_ = 0;
    bool zf_ = false;
    bool ready_ = false;
    std::uint32_t instructions_ = 0;

    bool fetch(std::uint8_t &value);
    bool fetch32(std::uint32_t &value);
    bool fetch64(std::uint64_t &value);
    bool push(std::uint64_t value);
    bool pop(std::uint64_t &value);
    std::optional<GuestStop> step();
    GuestCPUResult result(GuestStop status) const;
};

// Synthetic guest ELF + separately mapped, self-authored guest library.
// These are not PlayStation executables, system libraries, or firmware.
std::vector<std::uint8_t> makeGuestCPUFixture();
struct GuestCPUDiagnostic {
    GuestCPUResult cpu;
    std::uint32_t segments = 0;
    std::uint32_t imports = 0;
    std::uint64_t linkedAddress = 0;
    bool importReadOnly = false;
};
std::optional<GuestCPUDiagnostic> runGuestCPUDiagnostic();

} // namespace misaki
