#include "GuestX64Backend.hpp"

#include <cstdint>
#include <cstdlib>
#include <iostream>
#include <memory>
#include <vector>

namespace {
unsigned assertions = 0;
void check(bool condition, const char *message) {
    ++assertions;
    if (!condition) {
        std::cerr << "FAIL: " << message << '\n';
        std::exit(EXIT_FAILURE);
    }
}
void appendMov(std::vector<std::uint8_t> &code, unsigned reg,
               std::uint64_t immediate) {
    code.push_back(reg >= 8 ? 0x49 : 0x48);
    code.push_back(static_cast<std::uint8_t>(0xb8 + (reg & 7)));
    for (unsigned i = 0; i < 8; ++i)
        code.push_back(static_cast<std::uint8_t>(immediate >> (i * 8)));
}
misaki::GuestMemory codeMemory(const std::vector<std::uint8_t> &program,
                               std::uint64_t base = 0x1000) {
    misaki::GuestMemory memory;
    check(memory.map(base, program.size(), misaki::permission::read | misaki::permission::write),
          "map test code");
    check(memory.writeBytes(base, program.data(), program.size()), "initialize code");
    check(memory.protect(base, program.size(),
                         misaki::permission::read | misaki::permission::execute),
          "make code read+execute");
    return memory;
}
misaki::X64ExecutionResult run(const std::vector<std::uint8_t> &code,
                               std::uint32_t steps = 100) {
    misaki::PortableX64Backend cpu(codeMemory(code), 0x1000);
    return cpu.run(steps);
}
}

int main() {
    using namespace misaki;
    const auto diagnostic = runX64BackendDiagnostic();
    check(bool(diagnostic), "backend diagnostic prepared");
    check(diagnostic && diagnostic->execution.stop == X64Stop::halted,
          "backend diagnostic halts");
    check(diagnostic && diagnostic->execution.rax() == 44, "linked guest returns 44");
    check(diagnostic && diagnostic->execution.state.instructions == 13,
          "13 guest instructions executed");
    check(diagnostic && diagnostic->execution.stackRestored(), "stack restored");
    check(diagnostic && !diagnostic->execution.state.zeroFlag, "guest function ADD updates ZF");
    check(diagnostic && diagnostic->linkedAddress == 0x3000,
          "import resolved to guest library");
    check(diagnostic && diagnostic->importReadOnly && diagnostic->importedSymbols == 1,
          "import read-only after binding");

    // Dedicated backend instances can be accessed through the common interface.
    {
        GuestMemory memory = codeMemory({0x90, 0xf4});
        std::unique_ptr<IX64ExecutionBackend> backend =
            std::make_unique<PortableX64Backend>(std::move(memory), 0x1000);
        const auto result = backend->run(5);
        check(result.stop == X64Stop::halted && result.state.instructions == 2,
              "execution backend interface works");
    }

    // Immediate arithmetic, sign extension, register-reg arithmetic.
    {
        std::vector<std::uint8_t> bytes;
        appendMov(bytes, 0, 0xffffffffffffffffULL);
        bytes.insert(bytes.end(), {0x48, 0x05, 1, 0, 0, 0, 0xf4});
        const auto r = run(bytes);
        check(r.rax() == 0 && r.state.zeroFlag && r.state.carryFlag,
              "64-bit overflow wraps, ZF and CF updated");
        check(!r.state.overflowFlag, "unsigned carry does not imply signed overflow");
    }
    {
        std::vector<std::uint8_t> bytes;
        appendMov(bytes, 0, 0x7fffffffffffffffULL);
        bytes.insert(bytes.end(), {0x48, 0x05, 1, 0, 0, 0, 0xf4});
        const auto r = run(bytes);
        check(r.state.overflowFlag && r.state.signFlag,
              "signed addition overflow sets OF and SF");
        check(!r.state.carryFlag, "no unsigned carry on signed overflow");
    }
    {
        std::vector<std::uint8_t> bytes;
        appendMov(bytes, 0, 40);
        bytes.insert(bytes.end(), {0x48, 0x83, 0xc0, 0x02, // add rax,2
                                   0x48, 0x83, 0xe8, 0x01, // sub rax,1
                                   0x48, 0x83, 0xf8, 0x29, // cmp rax,41
                                   0xf4});
        const auto r = run(bytes);
        check(r.stop == X64Stop::halted && r.rax() == 41 && r.state.zeroFlag,
              "immediate group add/sub/cmp");
    }
    {
        std::vector<std::uint8_t> bytes;
        appendMov(bytes, 0, 0xffffffffffffffffULL);
        bytes.insert(bytes.end(), {0x48, 0x83, 0xc0, 1, 0xf4});
        check(run(bytes).state.zeroFlag, "sign-extended imm8 addition");
    }
    {
        std::vector<std::uint8_t> bytes;
        appendMov(bytes, 0, 1);
        bytes.insert(bytes.end(), {0x48, 0x2d, 3, 0, 0, 0, 0xf4});
        const auto r = run(bytes);
        check(r.rax() == std::uint64_t(-2) && r.state.carryFlag,
              "subtract sets unsigned borrow flag");
    }
    {
        std::vector<std::uint8_t> bytes;
        appendMov(bytes, 0, 7);
        appendMov(bytes, 3, 5);
        bytes.insert(bytes.end(), {0x48, 0x01, 0xd8, // add rax,rbx
                                   0x48, 0x29, 0xd8, // sub rax,rbx
                                   0x48, 0x31, 0xd8, // xor rax,rbx
                                   0x48, 0x85, 0xc0, // test rax,rax
                                   0xf4});
        const auto r = run(bytes);
        check(r.rax() == 2 && !r.state.zeroFlag, "register arithmetic and TEST");
    }
    {
        std::vector<std::uint8_t> bytes;
        appendMov(bytes, 8, 32);
        appendMov(bytes, 9, 10);
        bytes.insert(bytes.end(), {0x4d, 0x01, 0xc8, // add r8,r9
                                   0x4c, 0x89, 0xc0, // mov rax,r8
                                   0xf4});
        const auto r = run(bytes);
        check(r.rax() == 42 && r.state.registers[8] == 42,
              "REX extended registers R8 and R9");
    }

    // Conditional branches read actual x86 flags.
    {
        std::vector<std::uint8_t> bytes;
        appendMov(bytes, 0, 2);
        bytes.insert(bytes.end(), {0x48, 0x3d, 3, 0, 0, 0,
                                   0x7c, 0x02, 0xcc, 0x90, 0xf4});
        const auto r = run(bytes);
        check(r.stop == X64Stop::halted && r.state.instructions == 4,
              "JL taken for signed less-than");
        check(r.state.signFlag && !r.state.zeroFlag, "CMP flags retained by Jcc");
    }
    {
        std::vector<std::uint8_t> bytes;
        appendMov(bytes, 0, 5);
        bytes.insert(bytes.end(), {0x48, 0x3d, 6, 0, 0, 0,
                                   0x0f, 0x85, 2, 0, 0, 0, 0xcc, 0x90, 0xf4});
        check(run(bytes).stop == X64Stop::halted, "near JNE branches on ZF=0");
    }
    check(run({0xeb, 0x01, 0xcc, 0xf4}).stop == X64Stop::halted,
          "short relative JMP");
    check(run({0xe9, 0x01, 0, 0, 0, 0xcc, 0xf4}).stop == X64Stop::halted,
          "near relative JMP");

    // x86-64 memory operands including SIB, RIP-relative, and displacement.
    {
        std::vector<std::uint8_t> bytes;
        appendMov(bytes, 0, 42);
        bytes.insert(bytes.end(), {0x48, 0x89, 0x44, 0x24, 0xf8, // mov [rsp-8],rax
                                   0x48, 0x8b, 0x4c, 0x24, 0xf8, // mov rcx,[rsp-8]
                                   0x48, 0x89, 0xc8,             // mov rax,rcx
                                   0xf4});
        const auto r = run(bytes);
        check(r.rax() == 42 && r.stackRestored(), "SIB addressing on stack");
    }
    {
        const auto r = run({0x48, 0x8b, 0x05, 1, 0, 0, 0, 0xf4,
                            42, 0, 0, 0, 0, 0, 0, 0});
        check(r.stop == X64Stop::halted && r.rax() == 42,
              "RIP-relative MOV loads guest data");
    }
    {
        auto memory = codeMemory({0x48, 0xc7, 0x05, 1, 0, 0, 0, 42, 0, 0, 0,
                                  0xf4, 0, 0, 0, 0, 0, 0, 0, 0});
        // Test data is deliberately nonwritable: write must fault, not change bytes.
        PortableX64Backend backend(std::move(memory), 0x1000);
        check(backend.run(20).stop == X64Stop::memoryFault,
              "RIP-relative C7 respects RX protection");
    }
    {
        // LEA RAX,[RIP+0] reads an effective address, not memory contents.
        const auto r = run({0x48, 0x8d, 0x05, 0, 0, 0, 0, 0xf4});
        check(r.rax() == 0x1007, "LEA computes RIP-relative effective address");
    }

    // Arithmetic IMUL signed overflow without native signed overflow UB.
    {
        std::vector<std::uint8_t> bytes;
        appendMov(bytes, 0, 6);
        appendMov(bytes, 3, 7);
        bytes.insert(bytes.end(), {0x48, 0x0f, 0xaf, 0xc3, 0xf4});
        const auto r = run(bytes);
        check(r.rax() == 42 && !r.state.overflowFlag, "signed IMUL no overflow");
    }
    {
        std::vector<std::uint8_t> bytes;
        appendMov(bytes, 0, 0x7fffffffffffffffULL);
        appendMov(bytes, 3, 3);
        bytes.insert(bytes.end(), {0x48, 0x0f, 0xaf, 0xc3, 0xf4});
        check(run(bytes).state.overflowFlag, "signed IMUL overflow detected");
    }

    // Function frames, relative CALL, and stack restoration.
    {
        const auto r = run({0xe8, 1, 0, 0, 0, 0xf4,
                            0x48, 0xb8, 42, 0, 0, 0, 0, 0, 0, 0, 0xc3});
        check(r.stop == X64Stop::halted && r.rax() == 42 && r.stackRestored(),
              "direct CALL/RET returns without losing stack");
    }
    {
        const auto r = run({0x6a, 0xff, 0x58, 0xf4});
        check(r.rax() == UINT64_MAX && r.stackRestored(),
              "PUSH imm8 sign-extended then POP restores SP");
    }
    {
        const auto r = run({0xc3});
        check(r.stop == X64Stop::stackFault, "RET without call is stack fault");
    }

    // Fault handling and instruction budgets stay deterministic, non-JIT.
    check(run({0xcc}).stop == X64Stop::invalidOpcode,
          "int3 is not silently emulated as NOP");
    check(run({0x48, 0x90}).stop == X64Stop::invalidOpcode,
          "unsupported prefixed NOP safely rejected");
    check(run({0x48}).stop == X64Stop::memoryFault,
          "truncated x86 REX sequence faults safely");
    check(run({0xeb, 0xfe}, 7).stop == X64Stop::stepLimit,
          "infinite loop cut off by step budget");
    check(run({0x90}, 0).stop == X64Stop::stepLimit,
          "zero instruction budget rejected");
    check(run({0x90}, 100001).stop == X64Stop::stepLimit,
          "unbounded instruction request rejected");
    {
        GuestMemory memory;
        check(memory.map(0x1000, 1, permission::read), "map read-only code probe");
        PortableX64Backend cpu(std::move(memory), 0x1000);
        check(cpu.run(4).stop == X64Stop::memoryFault,
              "CPU cannot execute non-X mapped pages");
    }
    {
        PortableX64Backend cpu(codeMemory({0x90, 0xf4}), 0x1000);
        const auto first = cpu.run(1);
        const auto second = cpu.run(1);
        check(first.stop == X64Stop::stepLimit && first.state.instructions == 1,
              "single instruction quantum yields on step budget");
        check(second.stop == X64Stop::halted && second.state.instructions == 2,
              "run resumes after the previous quantum");
        check(cpu.run(1).state.instructions == 2,
              "halted backend remains halted on subsequent run");
    }

    std::cout << "PASS: " << assertions << " portable x86-64 backend assertions\n";
}
