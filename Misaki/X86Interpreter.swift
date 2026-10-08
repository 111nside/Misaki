import Foundation

/// Restricted, non-JIT userspace interpreter for self-authored ELF64 tests.
/// Not a full AMD Jaguar CPU or PS4 firmware implementation.
struct X86Interpreter {
    var memory = VirtualMemory()

    // x86 encoding order: RAX RCX RDX RBX RSP RBP RSI RDI R8...R15.
    private var registers = [UInt64](repeating: 0, count: 16)
    private(set) var rip: UInt64 = 0
    private(set) var zeroFlag = false
    private(set) var isHalted = false
    private(set) var exitCode: UInt64?
    private(set) var consoleOutput = ""
    private(set) var executedInstructions = 0

    var rax: UInt64 { registers[0] }
    var rcx: UInt64 { registers[1] }
    var rsp: UInt64 { registers[4] }
    var rdi: UInt64 { registers[7] }
    var rsi: UInt64 { registers[6] }

    private static let stackBottom: UInt64 = 0x7000_0000
    private static let stackSize = 64 * 1024

    mutating func load(_ program: [UInt8], at address: UInt64 = 0x1000) {
        memory = VirtualMemory()
        memory.load(program, at: address)
        reset(entry: address)
        // Raw test program caller provides trusted, small addresses.
        try? installStack()
    }

    mutating func loadELF(_ data: Data) throws {
        let image = try ELFLoader.load(data)
        memory = image.memory
        reset(entry: image.entryPoint)
        try installStack()
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

    private mutating func installStack() throws {
        try memory.map([UInt8](repeating: 0, count: Self.stackSize),
                       at: Self.stackBottom, permissions: [.read, .write])
        registers[4] = Self.stackBottom + UInt64(Self.stackSize)
    }

    private mutating func fetch() throws -> UInt8 {
        let byte = try memory.fetch8(rip)
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
        rip = rip &+ UInt64(bitPattern: offset)
    }

    private mutating func push(_ value: UInt64) throws {
        guard registers[4] >= 8 else { throw EmulatorError.invalidMemoryMapping }
        let newSP = registers[4] - 8
        try memory.write64(newSP, value: value)
        registers[4] = newSP
    }

    private mutating func pop() throws -> UInt64 {
        let value = try memory.read64(registers[4])
        registers[4] = registers[4] &+ 8
        return value
    }

    /// Only register-to-register ModRM supported in this milestone.
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
        // Research-only ABI, NOT PS4 or FreeBSD kernel system calls.
        switch registers[0] {
        case 1:
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
        case 60:
            exitCode = registers[7]
            isHalted = true
        default: throw EmulatorError.unsupportedSyscall(registers[0])
        }
    }

    mutating func step() throws {
        guard !isHalted else { throw EmulatorError.halted }
        let opcode = try fetch()
        switch opcode {
        case 0x90: break
        case 0xF4: isHalted = true
        case 0x0F:
            let second = try fetch()
            guard second == 0x05 else { throw EmulatorError.invalidInstruction(second) }
            try handleDemoSyscall()
        case 0x50...0x57: try push(registers[Int(opcode - 0x50)])
        case 0x58...0x5F: registers[Int(opcode - 0x58)] = try pop()
        case 0x68: // PUSH imm32, sign extended
            try push(UInt64(bitPattern: Int64(Int32(bitPattern: try fetch32()))))
        case 0x6A: // PUSH imm8, sign extended
            try push(UInt64(bitPattern: Int64(Int8(bitPattern: try fetch()))))
        case 0xC3: rip = try pop() // RET
        case 0xE8: // CALL rel32, pushes address following instruction
            let relative = Int64(Int32(bitPattern: try fetch32()))
            try push(rip)
            branch(relative: relative)
        case 0xEB: branch(relative: Int64(Int8(bitPattern: try fetch())))
        case 0xE9: branch(relative: Int64(Int32(bitPattern: try fetch32())))
        case 0x74:
            let offset = Int64(Int8(bitPattern: try fetch()))
            if zeroFlag { branch(relative: offset) }
        case 0x75:
            let offset = Int64(Int8(bitPattern: try fetch()))
            if !zeroFlag { branch(relative: offset) }
        case 0x48...0x4F:
            let rex = opcode
            let second = try fetch()
            // PUSH/POP of R8...R15 use REX.B, 64-bit by default.
            if second >= 0x50 && second <= 0x57 {
                let index = Int(second - 0x50) + ((rex & 1) != 0 ? 8 : 0)
                try push(registers[index])
            } else if second >= 0x58 && second <= 0x5F {
                let index = Int(second - 0x58) + ((rex & 1) != 0 ? 8 : 0)
                registers[index] = try pop()
            } else {
                guard (rex & 0x08) != 0 else { throw EmulatorError.invalidInstruction(rex) }
                switch second {
                case 0xB8...0xBF:
                    let index = Int(second - 0xB8) + ((rex & 1) != 0 ? 8 : 0)
                    registers[index] = try fetch64()
                case 0x05:
                    let signed = Int64(Int32(bitPattern: try fetch32()))
                    registers[0] = registers[0] &+ UInt64(bitPattern: signed)
                    zeroFlag = registers[0] == 0
                case 0x2D:
                    let signed = Int64(Int32(bitPattern: try fetch32()))
                    registers[0] = registers[0] &- UInt64(bitPattern: signed)
                    zeroFlag = registers[0] == 0
                case 0x3D:
                    let signed = Int64(Int32(bitPattern: try fetch32()))
                    zeroFlag = registers[0] == UInt64(bitPattern: signed)
                case 0x89:
                    let operands = try registerOperands(rex: rex)
                    registers[operands.rm] = registers[operands.reg]
                case 0x8B:
                    let operands = try registerOperands(rex: rex)
                    registers[operands.reg] = registers[operands.rm]
                case 0x31:
                    let operands = try registerOperands(rex: rex)
                    registers[operands.rm] ^= registers[operands.reg]
                    zeroFlag = registers[operands.rm] == 0
                case 0x33:
                    let operands = try registerOperands(rex: rex)
                    registers[operands.reg] ^= registers[operands.rm]
                    zeroFlag = registers[operands.reg] == 0
                default: throw EmulatorError.invalidInstruction(second)
                }
            }
        default: throw EmulatorError.invalidInstruction(opcode)
        }
        executedInstructions += 1
    }

    mutating func run(maxSteps: Int = 10_000) throws {
        guard maxSteps > 0 else { throw EmulatorError.stepLimit }
        for _ in 0..<maxSteps {
            if isHalted { return }
            try step()
        }
        if !isHalted { throw EmulatorError.stepLimit }
    }
}
