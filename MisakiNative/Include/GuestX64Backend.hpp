#pragma once

#include "MisakiCore.hpp"

#include <array>
#include <cstdint>
#include <optional>

namespace misaki {

// Non-JIT, deliberately restricted x86-64 interpreter used as a portable
// execution backend. It does not run PS4 kernel code, SELF, or games.
enum class X64Stop : std::uint8_t {
    halted, invalidOpcode, memoryFault, stackFault, stepLimit, yielded, exited, unsupportedSyscall
};

struct X64State {
    // Encoded order: RAX RCX RDX RBX RSP RBP RSI RDI R8..R15.
    std::array<std::uint64_t, 16> registers{};
    std::uint64_t rip = 0;
    std::uint64_t initialStack = 0;
    std::uint32_t instructions = 0;
    bool zeroFlag = false;
    bool signFlag = false;
    bool carryFlag = false;
    bool overflowFlag = false;
    bool parityFlag = false;
};

struct X64ExecutionResult {
    X64Stop stop = X64Stop::invalidOpcode;
    X64State state;
    std::uint64_t rax() const { return state.registers[0]; }
    bool stackRestored() const { return state.registers[4] == state.initialStack; }
};

// A stable execution contract for a future, separately implemented backend.
// The only working implementation in Milestone 11 is PortableX64Backend.
class IX64ExecutionBackend {
public:
    virtual ~IX64ExecutionBackend() = default;
    virtual X64ExecutionResult run(std::uint32_t maxInstructions) = 0;
};

// The service ABI is injectable; guest code never calls the iPhone OS directly.
enum class GuestServiceAction : std::uint8_t {
    resume, yield, exit, memoryFault, unsupported
};

class IGuestServiceDispatcher {
public:
    virtual ~IGuestServiceDispatcher() = default;
    virtual GuestServiceAction dispatch(GuestMemory &memory, X64State &state,
                                        std::uint32_t threadID) = 0;
};

class PortableX64Backend final : public IX64ExecutionBackend {
public:
    explicit PortableX64Backend(GuestMemory memory, std::uint64_t entry,
                                IGuestServiceDispatcher *services = nullptr,
                                std::uint32_t threadID = 1);
    X64ExecutionResult run(std::uint32_t maxInstructions = 1000) override;

private:
    struct Operand {
        bool isRegister = false;
        unsigned index = 0;
        std::uint64_t address = 0;
    };
    struct ModRM {
        unsigned reg = 0;
        unsigned group = 0;
        Operand rm;
    };

    GuestMemory memory_;
    IGuestServiceDispatcher *services_ = nullptr; // owned by enclosing scheduler
    std::uint32_t threadID_ = 1;
    X64State cpu_;
    bool stackReady_ = false;
    bool terminated_ = false;
    X64Stop lastStop_ = X64Stop::stepLimit;

    bool fetch8(std::uint8_t &out);
    bool fetch32(std::uint32_t &out);
    bool fetch64(std::uint64_t &out);
    bool decodeModRM(std::uint8_t rex, std::uint64_t immediateBytes,
                     ModRM &out);
    std::optional<std::uint64_t> readOperand(const Operand &op) const;
    bool writeOperand(const Operand &op, std::uint64_t value);
    bool push(std::uint64_t value);
    bool pop(std::uint64_t &value);
    void logicFlags(std::uint64_t value);
    void addFlags(std::uint64_t lhs, std::uint64_t rhs, std::uint64_t result);
    void subFlags(std::uint64_t lhs, std::uint64_t rhs, std::uint64_t result);
    bool branchCondition(unsigned code) const;
    std::optional<X64Stop> step();
    X64ExecutionResult result(X64Stop stop) const;
};

// Exercises the backend with x86-64 register arithmetic, stack frames,
// indirect guest imports, and control flow in executable-only guest memory.
struct X64BackendDiagnostic {
    X64ExecutionResult execution;
    std::uint32_t importedSymbols = 0;
    bool importReadOnly = false;
    std::uint64_t linkedAddress = 0;
};
std::optional<X64BackendDiagnostic> runX64BackendDiagnostic();

} // namespace misaki
