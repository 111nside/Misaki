import Foundation

/// Intentionally small, non-JIT x86-64 interpreter for independently developed test programs.
/// Supported opcodes: NOP (90), MOV RAX, imm64 (48 B8), ADD RAX, imm32 (48 05), HLT (F4).
struct X86Interpreter {
    var memory = VirtualMemory()
    private(set) var rip: UInt64 = 0
    private(set) var rax: UInt64 = 0
    private(set) var isHalted = false

    mutating func load(_ program: [UInt8], at address: UInt64 = 0x1000) {
        memory = VirtualMemory()
        memory.load(program, at: address)
        rip = address
        rax = 0
        isHalted = false
    }

    /// Loads an ELF64 test binary's PT_LOAD segments, then starts at e_entry.
    mutating func loadELF(_ data: Data) throws {
        let image = try ELFLoader.load(data)
        memory = image.memory
        rip = image.entryPoint
        rax = 0
        isHalted = false
    }

    private mutating func fetch() throws -> UInt8 {
        let value = try memory.read8(rip)
        rip &+= 1
        return value
    }
    private mutating func fetch64() throws -> UInt64 {
        var result: UInt64 = 0
        for shift in 0..<8 { result |= UInt64(try fetch()) << (shift * 8) }
        return result
    }
    private mutating func fetch32() throws -> UInt32 {
        var result: UInt32 = 0
        for shift in 0..<4 { result |= UInt32(try fetch()) << (shift * 8) }
        return result
    }
    mutating func step() throws {
        if isHalted { throw EmulatorError.halted }
        let opcode = try fetch()
        switch opcode {
        case 0x90: break
        case 0xF4: isHalted = true
        case 0x48:
            let second = try fetch()
            switch second {
            case 0xB8: rax = try fetch64()
            case 0x05:
                let imm = try fetch32()
                let signed = Int64(Int32(bitPattern: imm))
                rax = rax &+ UInt64(bitPattern: signed)
            default: throw EmulatorError.invalidInstruction(second)
            }
        default: throw EmulatorError.invalidInstruction(opcode)
        }
    }
    mutating func run(maxSteps: Int = 10000) throws {
        for _ in 0..<maxSteps {
            if isHalted { return }
            try step()
        }
        if !isHalted { throw EmulatorError.stepLimit }
    }
}
