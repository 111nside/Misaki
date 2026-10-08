import Foundation

/// An intentionally restricted, non-JIT x86-64 userspace interpreter.
/// This is a research test harness, not a PS4 CPU implementation.
/// Only selected 64-bit register instructions and a few branches are recognized.
struct X86Interpreter {
    var memory = VirtualMemory()

    // Register indices use the x86 encoding order:
    // RAX, RCX, RDX, RBX, RSP, RBP, RSI, RDI, R8...R15.
    private var registers = [UInt64](repeating: 0, count: 16)
    private(set) var rip: UInt64 = 0
    private(set) var zeroFlag = false
    private(set) var isHalted = false
    private(set) var exitCode: UInt64?
    private(set) var consoleOutput = ""
    private(set) var executedInstructions = 0

    var rax: UInt64 { registers[0] }
    var rdi: UInt64 { registers[7] }
    var rsi: UInt64 { registers[6] }

    mutating func load(_ program: [UInt8], at address: UInt64 = 0x1000) {
        memory = VirtualMemory()
        memory.load(program, at: address)
        reset(entry: address)
    }

    /// Maps PT_LOAD segments from a small, unencrypted x86-64 ET_EXEC ELF.
    mutating func loadELF(_ data: Data) throws {
        let image = try ELFLoader.load(data)
        memory = image.memory
        reset(entry: image.entryPoint)
    }

    private mutating func reset(entry: UInt64) {
        registers = [UInt64](repeating: 0, count: 16)
        rip = entry
        zeroFlag = false
        isHalted = false
        exitCode = nil
        consoleOutput = ""
        executedInstructions = 0
    }

    private mutating func fetch() throws -> UInt8 {
        let byte = try memory.read8(rip)
        rip &+= 1
        return byte
    }
    private mutating func fetch32() throws -> UInt32 {
        var value: UInt32 = 0
        for shift in 0..<4 { value |= UInt32(try fetch()) << (shift * 8) }
        return value
    }
    private mutating func fetch64() throws -> UInt64 {
        var value: UInt64 = 0
        for shift in 0..<8 { value |= UInt64(try fetch()) << (shift * 8) }
        return value
    }
    private mutating func branch(relative offset: Int64) {
        rip = UInt64(bitPattern: Int64(bitPattern: rip) &+ offset)
    }

    /// For these instruction forms, only ModRM register-to-register operands
    /// are accepted. No SIB, displacement, or effective-address decoding.
    private mutating func registerOperands(rex: UInt8) throws -> (reg: Int, rm: Int) {
        let modRM = try fetch()
        guard (modRM & 0xC0) == 0xC0 else {
            throw EmulatorError.invalidInstruction(modRM)
        }
        let reg = Int((modRM >> 3) & 7) + ((rex & 4) != 0 ? 8 : 0)
        let rm = Int(modRM & 7) + ((rex & 1) != 0 ? 8 : 0)
        return (reg, rm)
    }

    private mutating func handleDemoSyscall() throws {
        // Demonstration ABI ONLY. These numbers and semantics are NOT
        // PS4/FreeBSD-compatible kernel system calls.
        switch registers[0] {
        case 1:  // demo.write(pointer=RDI, length=RSI)
            let address = registers[7]
            let count = registers[6]
            guard count <= 4096, address <= UInt64.max - count else {
                throw EmulatorError.invalidSyscallArguments
            }
            var bytes: [UInt8] = []
            bytes.reserveCapacity(Int(count))
            for i in 0..<Int(count) {
                bytes.append(try memory.read8(address + UInt64(i)))
            }
            consoleOutput += String(decoding: bytes, as: UTF8.self)
            registers[0] = count
        case 60: // demo.exit(status=RDI)
            exitCode = registers[7]
            isHalted = true
        default:
            throw EmulatorError.unsupportedSyscall(registers[0])
        }
    }

    mutating func step() throws {
        guard !isHalted else { throw EmulatorError.halted }
        let opcode = try fetch()
        switch opcode {
        case 0x90: // NOP
            break
        case 0xF4: // HLT: diagnostic only
            isHalted = true
        case 0x0F:
            let second = try fetch()
            guard second == 0x05 else { throw EmulatorError.invalidInstruction(second) }
            try handleDemoSyscall()
        case 0xEB: // JMP rel8
            branch(relative: Int64(Int8(bitPattern: try fetch())))
        case 0xE9: // JMP rel32
            branch(relative: Int64(Int32(bitPattern: try fetch32())))
        case 0x74: // JE rel8
            let offset = Int64(Int8(bitPattern: try fetch()))
            if zeroFlag { branch(relative: offset) }
        case 0x75: // JNE rel8
            let offset = Int64(Int8(bitPattern: try fetch()))
            if !zeroFlag { branch(relative: offset) }
        case 0x48...0x4F: // Require REX.W (no 32-bit operand forms yet).
            let rex = opcode
            guard (rex & 0x08) != 0 else { throw EmulatorError.invalidInstruction(rex) }
            let second = try fetch()
            switch second {
            case 0xB8...0xBF: // MOV reg64, imm64
                let index = Int(second - 0xB8) + ((rex & 1) != 0 ? 8 : 0)
                registers[index] = try fetch64()
            case 0x05: // ADD RAX, imm32 (sign extended)
                let signed = Int64(Int32(bitPattern: try fetch32()))
                registers[0] = registers[0] &+ UInt64(bitPattern: signed)
                zeroFlag = registers[0] == 0
            case 0x2D: // SUB RAX, imm32
                let signed = Int64(Int32(bitPattern: try fetch32()))
                registers[0] = registers[0] &- UInt64(bitPattern: signed)
                zeroFlag = registers[0] == 0
            case 0x3D: // CMP RAX, imm32
                let signed = Int64(Int32(bitPattern: try fetch32()))
                zeroFlag = registers[0] == UInt64(bitPattern: signed)
            case 0x89: // MOV r/m64, r64 (register-register only)
                let operands = try registerOperands(rex: rex)
                registers[operands.rm] = registers[operands.reg]
            case 0x8B: // MOV r64, r/m64 (register-register only)
                let operands = try registerOperands(rex: rex)
                registers[operands.reg] = registers[operands.rm]
            case 0x31: // XOR r/m64, r64 (register-register only)
                let operands = try registerOperands(rex: rex)
                registers[operands.rm] ^= registers[operands.reg]
                zeroFlag = registers[operands.rm] == 0
            case 0x33: // XOR r64, r/m64 (register-register only)
                let operands = try registerOperands(rex: rex)
                registers[operands.reg] ^= registers[operands.rm]
                zeroFlag = registers[operands.reg] == 0
            default:
                throw EmulatorError.invalidInstruction(second)
            }
        default:
            throw EmulatorError.invalidInstruction(opcode)
        }
        executedInstructions += 1
    }

    mutating func run(maxSteps: Int = 10_000) throws {
        // Avoid unbounded loops on untrusted / malformed test inputs.
        guard maxSteps > 0 else { throw EmulatorError.stepLimit }
        for _ in 0..<maxSteps {
            if isHalted { return }
            try step()
        }
        if !isHalted { throw EmulatorError.stepLimit }
    }
}
