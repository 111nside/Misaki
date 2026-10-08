import Foundation

/// Deliberately restricted, non-JIT x86-64 userspace interpreter for small,
/// independently authored ELF64 tests. This is NOT PS4 system-software emulation.
struct X86Interpreter {
    var memory = VirtualMemory()

    // x86 register encoding order: RAX RCX RDX RBX RSP RBP RSI RDI R8...R15.
    private var registers = [UInt64](repeating: 0, count: 16)
    private var xmm = [[UInt8]](repeating: [UInt8](repeating: 0, count: 16), count: 16)
    private(set) var rip: UInt64 = 0
    private(set) var zeroFlag = false
    private(set) var signFlag = false
    private(set) var carryFlag = false
    private(set) var overflowFlag = false
    private(set) var parityFlag = false
    private(set) var isHalted = false
    private(set) var exitCode: UInt64?
    private(set) var consoleOutput = ""
    private(set) var executedInstructions = 0

    var rax: UInt64 { registers[0] }
    var rcx: UInt64 { registers[1] }
    var rdx: UInt64 { registers[2] }
    var rsp: UInt64 { registers[4] }
    var rdi: UInt64 { registers[7] }
    var rsi: UInt64 { registers[6] }
    func register(_ index: Int) -> UInt64 { registers[index] }
    func xmmBytes(_ index: Int) -> [UInt8] { xmm[index] }

    private static let stackBottom: UInt64 = 0x7000_0000
    private static let stackSize = 64 * 1024

    mutating func load(_ program: [UInt8], at address: UInt64 = 0x1000) {
        memory = VirtualMemory()
        memory.load(program, at: address)
        reset(entry: address)
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
        xmm = [[UInt8]](repeating: [UInt8](repeating: 0, count: 16), count: 16)
        rip = entry
        zeroFlag = false
        signFlag = false
        carryFlag = false
        overflowFlag = false
        parityFlag = false
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

    private enum Operand {
        case register(Int)
        case memory(UInt64)
    }

    private struct DecodedModRM {
        let reg: Int
        let group: Int
        let rm: Operand
    }

    /// Selected 64-bit ModRM operands, including SIB, displacement and RIP-relative memory.
    private mutating func decodeModRM(rex: UInt8, trailingImmediateBytes: UInt64 = 0) throws -> DecodedModRM {
        let byte = try fetch()
        let mode = Int(byte >> 6)
        let group = Int((byte >> 3) & 7)
        let reg = group + ((rex & 4) != 0 ? 8 : 0)
        let rm = Int(byte & 7)
        if mode == 3 {
            return DecodedModRM(reg: reg, group: group,
                                rm: .register(rm + ((rex & 1) != 0 ? 8 : 0)))
        }

        var base: UInt64 = 0
        var index: UInt64 = 0
        var isRIPRelative = false
        var needsDisp32 = false
        if rm == 4 { // SIB
            let sib = try fetch()
            let scale = Int(sib >> 6)
            let indexField = Int((sib >> 3) & 7)
            let baseField = Int(sib & 7)
            if indexField != 4 || (rex & 2) != 0 {
                let n = indexField + ((rex & 2) != 0 ? 8 : 0)
                index = registers[n] &* (UInt64(1) << scale)
            }
            if mode == 0 && baseField == 5 {
                needsDisp32 = true // SIB no-base form, not RIP-relative
            } else {
                base = registers[baseField + ((rex & 1) != 0 ? 8 : 0)]
            }
        } else if mode == 0 && rm == 5 {
            needsDisp32 = true
            isRIPRelative = true
        } else {
            base = registers[rm + ((rex & 1) != 0 ? 8 : 0)]
        }

        let displacement: Int64
        switch mode {
        case 1: displacement = Int64(Int8(bitPattern: try fetch()))
        case 2: displacement = Int64(Int32(bitPattern: try fetch32()))
        default:
            displacement = needsDisp32 ? Int64(Int32(bitPattern: try fetch32())) : 0
        }
        if isRIPRelative { base = rip &+ trailingImmediateBytes } // RIP points after full instruction
        let address = base &+ index &+ UInt64(bitPattern: displacement)
        return DecodedModRM(reg: reg, group: group, rm: .memory(address))
    }

    private func read(_ operand: Operand) throws -> UInt64 {
        switch operand {
        case .register(let n): return registers[n]
        case .memory(let address): return try memory.read64(address)
        }
    }

    private mutating func write(_ operand: Operand, _ value: UInt64) throws {
        switch operand {
        case .register(let n): registers[n] = value
        case .memory(let address): try memory.write64(address, value: value)
        }
    }

    private func readXMM(_ operand: Operand, aligned: Bool = false) throws -> [UInt8] {
        switch operand {
        case .register(let n): return xmm[n]
        case .memory(let address):
            if aligned && address % 16 != 0 { throw EmulatorError.protectionFault(address) }
            guard address <= UInt64.max - 15 else { throw EmulatorError.invalidMemoryMapping }
            return try (0..<16).map { try memory.read8(address + UInt64($0)) }
        }
    }

    private mutating func writeXMM(_ operand: Operand, _ value: [UInt8], aligned: Bool = false) throws {
        switch operand {
        case .register(let n): xmm[n] = value
        case .memory(let address):
            if aligned && address % 16 != 0 { throw EmulatorError.protectionFault(address) }
            try memory.writeBytes(address, values: value)
        }
    }

    private mutating func updateResult(_ value: UInt64) {
        zeroFlag = value == 0
        signFlag = (value & (UInt64(1) << 63)) != 0
        parityFlag = UInt8(truncatingIfNeeded: value).nonzeroBitCount % 2 == 0
    }

    private mutating func setAddFlags(_ a: UInt64, _ b: UInt64, _ result: UInt64) {
        updateResult(result)
        carryFlag = result < a
        overflowFlag = ((~(a ^ b) & (a ^ result)) & (UInt64(1) << 63)) != 0
    }

    private mutating func setSubFlags(_ a: UInt64, _ b: UInt64, _ result: UInt64) {
        updateResult(result)
        carryFlag = a < b
        overflowFlag = (((a ^ b) & (a ^ result)) & (UInt64(1) << 63)) != 0
    }

    private mutating func setLogicFlags(_ result: UInt64) {
        updateResult(result)
        carryFlag = false
        overflowFlag = false
    }

    private func condition(_ code: UInt8) -> Bool {
        switch code & 15 {
        case 0: return overflowFlag
        case 1: return !overflowFlag
        case 2: return carryFlag
        case 3: return !carryFlag
        case 4: return zeroFlag
        case 5: return !zeroFlag
        case 6: return carryFlag || zeroFlag
        case 7: return !carryFlag && !zeroFlag
        case 8: return signFlag
        case 9: return !signFlag
        case 10: return parityFlag
        case 11: return !parityFlag
        case 12: return signFlag != overflowFlag
        case 13: return signFlag == overflowFlag
        case 14: return zeroFlag || (signFlag != overflowFlag)
        default: return !zeroFlag && (signFlag == overflowFlag)
        }
    }

    private mutating func handleDemoSyscall() throws {
        // This intentionally is not the PS4/FreeBSD system call ABI.
        switch registers[0] {
        case 1: // demo.write(pointer=RDI, length=RSI)
            let address = registers[7]
            let count = registers[6]
            guard count <= 4096, address <= UInt64.max - count else {
                throw EmulatorError.invalidSyscallArguments
            }
            let bytes = try (0..<Int(count)).map { try memory.read8(address + UInt64($0)) }
            consoleOutput += String(decoding: bytes, as: UTF8.self)
            registers[0] = count
        case 60: // demo.exit(status=RDI)
            exitCode = registers[7]
            isHalted = true
        case 0x100: // demo.pageSize: deterministic toy service
            registers[0] = 4096
        default: throw EmulatorError.unsupportedSyscall(registers[0])
        }
    }

    private mutating func executeSSE(second: UInt8, mandatory: UInt8?, rex: UInt8) throws {
        switch (mandatory, second) {
        case (0xF3, 0x6F): // MOVDQU xmm, xmm/m128
            let decoded = try decodeModRM(rex: rex)
            xmm[decoded.reg] = try readXMM(decoded.rm)
        case (0xF3, 0x7F): // MOVDQU xmm/m128, xmm
            let decoded = try decodeModRM(rex: rex)
            try writeXMM(decoded.rm, xmm[decoded.reg])
        case (0x66, 0x6F): // MOVDQA xmm, xmm/m128 (aligned)
            let decoded = try decodeModRM(rex: rex)
            xmm[decoded.reg] = try readXMM(decoded.rm, aligned: true)
        case (0x66, 0x7F): // MOVDQA xmm/m128, xmm (aligned)
            let decoded = try decodeModRM(rex: rex)
            try writeXMM(decoded.rm, xmm[decoded.reg], aligned: true)
        case (0x66, 0xEF): // PXOR xmm, xmm/m128
            let decoded = try decodeModRM(rex: rex)
            let rhs = try readXMM(decoded.rm)
            for i in 0..<16 { xmm[decoded.reg][i] ^= rhs[i] }
        case (0xF3, 0x10): // MOVSS xmm, xmm/m32, preserve upper bits (except reg-reg MOVSS)
            let decoded = try decodeModRM(rex: rex)
            switch decoded.rm {
            case .register(let r):
                let low = Array(xmm[r][0..<4])
                for i in 0..<4 { xmm[decoded.reg][i] = low[i] }
            case .memory(let a):
                guard a <= UInt64.max - 3 else { throw EmulatorError.invalidMemoryMapping }
                let low = try (0..<4).map { try memory.read8(a + UInt64($0)) }
                xmm[decoded.reg] = low + [UInt8](repeating: 0, count: 12)
            }
        case (0xF3, 0x58), (0xF3, 0x59): // ADDSS, MULSS
            let decoded = try decodeModRM(rex: rex)
            let rhsBytes: [UInt8]
            switch decoded.rm {
            case .register(let r): rhsBytes = xmm[r]
            case .memory(let a):
                guard a <= UInt64.max - 3 else { throw EmulatorError.invalidMemoryMapping }
                rhsBytes = try (0..<4).map { try memory.read8(a + UInt64($0)) }
            }
            func low32(_ b: [UInt8]) -> UInt32 {
                (0..<4).reduce(UInt32(0)) { $0 | (UInt32(b[$1]) << ($1 * 8)) }
            }
            let left = Float(bitPattern: low32(xmm[decoded.reg]))
            let right = Float(bitPattern: low32(rhsBytes))
            let result = (second == 0x58 ? left + right : left * right).bitPattern
            for i in 0..<4 { xmm[decoded.reg][i] = UInt8(truncatingIfNeeded: result >> (i * 8)) }
        default: throw EmulatorError.invalidInstruction(second)
        }
    }

    private mutating func executeInteger(second: UInt8, rex: UInt8) throws {
        switch second {
        case 0xB8...0xBF: // MOV reg64, imm64
            let n = Int(second - 0xB8) + ((rex & 1) != 0 ? 8 : 0)
            registers[n] = try fetch64()
        case 0x05, 0x2D, 0x3D: // ADD/SUB/CMP RAX, sign-extended imm32
            let rhs = UInt64(bitPattern: Int64(Int32(bitPattern: try fetch32())))
            let a = registers[0]
            if second == 0x05 {
                let r = a &+ rhs
                registers[0] = r
                setAddFlags(a, rhs, r)
            } else {
                let r = a &- rhs
                if second == 0x2D { registers[0] = r }
                setSubFlags(a, rhs, r)
            }
        case 0x89, 0x8B, 0x8D, 0x01, 0x03, 0x29, 0x2B, 0x39, 0x3B, 0x31, 0x33, 0x85:
            let d = try decodeModRM(rex: rex)
            if second == 0x8D { // LEA only with memory effective address
                guard case .memory(let address) = d.rm else { throw EmulatorError.invalidInstruction(second) }
                registers[d.reg] = address
                return
            }
            if second == 0x89 { try write(d.rm, registers[d.reg]); return }
            if second == 0x8B { registers[d.reg] = try read(d.rm); return }
            let a: UInt64
            let b: UInt64
            let dest: Operand
            if [UInt8(0x03), 0x2B, 0x3B, 0x33].contains(second) {
                a = registers[d.reg]; b = try read(d.rm); dest = .register(d.reg)
            } else {
                a = try read(d.rm); b = registers[d.reg]; dest = d.rm
            }
            switch second {
            case 0x01, 0x03:
                let value = a &+ b
                try write(dest, value)
                setAddFlags(a, b, value)
            case 0x29, 0x2B, 0x39, 0x3B:
                let value = a &- b
                if second == 0x29 || second == 0x2B { try write(dest, value) }
                setSubFlags(a, b, value)
            case 0x31, 0x33:
                let value = a ^ b
                try write(dest, value)
                setLogicFlags(value)
            default: setLogicFlags(a & b) // TEST
            }
        case 0x81, 0x83, 0xC7: // Group1: ADD /0, SUB /5, CMP /7; C7 /0 MOV
            let d = try decodeModRM(rex: rex, trailingImmediateBytes: second == 0x83 ? 1 : 4)
            let rhs: UInt64
            if second == 0x83 {
                rhs = UInt64(bitPattern: Int64(Int8(bitPattern: try fetch())))
            } else {
                rhs = UInt64(bitPattern: Int64(Int32(bitPattern: try fetch32())))
            }
            if second == 0xC7 {
                guard d.group == 0 else { throw EmulatorError.invalidInstruction(second) }
                try write(d.rm, rhs)
                return
            }
            let lhs = try read(d.rm)
            switch d.group {
            case 0:
                let r = lhs &+ rhs
                try write(d.rm, r)
                setAddFlags(lhs, rhs, r)
            case 5:
                let r = lhs &- rhs
                try write(d.rm, r)
                setSubFlags(lhs, rhs, r)
            case 7: setSubFlags(lhs, rhs, lhs &- rhs)
            default: throw EmulatorError.invalidInstruction(second)
            }
        case 0xF7: // 64-bit unsigned MUL, signed IMUL, DIV, restricted IDIV
            let d = try decodeModRM(rex: rex)
            let operand = try read(d.rm)
            switch d.group {
            case 4: // MUL RDX:RAX = RAX * r/m64
                let result = registers[0].multipliedFullWidth(by: operand)
                registers[0] = result.low
                registers[2] = result.high
                carryFlag = result.high != 0
                overflowFlag = carryFlag
            case 5: // IMUL RDX:RAX = RAX * r/m64
                let result = Int64(bitPattern: registers[0]).multipliedFullWidth(by: Int64(bitPattern: operand))
                registers[0] = result.low
                registers[2] = UInt64(bitPattern: result.high)
                let signExtension: Int64 = Int64(bitPattern: result.low) < 0 ? -1 : 0
                carryFlag = result.high != signExtension
                overflowFlag = carryFlag
            case 6: // DIV (128-bit unsigned dividend)
                guard operand != 0, registers[2] < operand else { throw EmulatorError.divideError }
                let result = operand.dividingFullWidth((high: registers[2], low: registers[0]))
                registers[0] = result.quotient
                registers[2] = result.remainder
            case 7: // IDIV: support sign-extended 64-bit dividend only in prototype
                let dividend = Int64(bitPattern: registers[0])
                let divisor = Int64(bitPattern: operand)
                let expectedHigh: UInt64 = dividend < 0 ? UInt64.max : 0
                guard divisor != 0, registers[2] == expectedHigh,
                      !(dividend == Int64.min && divisor == -1) else { throw EmulatorError.divideError }
                registers[0] = UInt64(bitPattern: dividend / divisor)
                registers[2] = UInt64(bitPattern: dividend % divisor)
            default: throw EmulatorError.invalidInstruction(second)
            }
        case 0xFF: // CALL r/m64, JMP r/m64, PUSH r/m64
            let d = try decodeModRM(rex: rex)
            let target = try read(d.rm)
            switch d.group {
            case 2: try push(rip); rip = target
            case 4: rip = target
            case 6: try push(target)
            default: throw EmulatorError.invalidInstruction(second)
            }
        default: throw EmulatorError.invalidInstruction(second)
        }
    }

    mutating func step() throws {
        guard !isHalted else { throw EmulatorError.halted }
        var rex: UInt8 = 0
        var mandatory: UInt8? = nil
        var opcode = try fetch()
        var prefixCount = 0
        while opcode == 0x66 || opcode == 0xF3 || (opcode >= 0x40 && opcode <= 0x4F) {
            prefixCount += 1
            guard prefixCount <= 4 else { throw EmulatorError.invalidInstruction(opcode) }
            if opcode == 0x66 || opcode == 0xF3 { mandatory = opcode }
            else { rex = opcode }
            opcode = try fetch()
        }
        let rexW = (rex & 8) != 0
        switch opcode {
        case 0x90 where mandatory == nil: break
        case 0xF4 where mandatory == nil: isHalted = true // diagnostic only
        case 0x0F:
            let second = try fetch()
            if mandatory != nil {
                try executeSSE(second: second, mandatory: mandatory, rex: rex)
            } else if second == 0x05 { // toy syscall ABI, NOT PS4
                try handleDemoSyscall()
            } else if (0x80...0x8F).contains(second) {
                let offset = Int64(Int32(bitPattern: try fetch32()))
                if condition(second) { branch(relative: offset) }
            } else if second == 0xAF && rexW { // IMUL reg64, r/m64
                let d = try decodeModRM(rex: rex)
                let full = Int64(bitPattern: registers[d.reg]).multipliedFullWidth(by: Int64(bitPattern: try read(d.rm)))
                registers[d.reg] = full.low
                let signExtension: Int64 = Int64(bitPattern: full.low) < 0 ? -1 : 0
                carryFlag = full.high != signExtension
                overflowFlag = carryFlag
            } else {
                throw EmulatorError.invalidInstruction(second)
            }
        case 0x50...0x57 where mandatory == nil:
            try push(registers[Int(opcode - 0x50) + ((rex & 1) != 0 ? 8 : 0)])
        case 0x58...0x5F where mandatory == nil:
            registers[Int(opcode - 0x58) + ((rex & 1) != 0 ? 8 : 0)] = try pop()
        case 0x68 where mandatory == nil:
            try push(UInt64(bitPattern: Int64(Int32(bitPattern: try fetch32()))))
        case 0x6A where mandatory == nil:
            try push(UInt64(bitPattern: Int64(Int8(bitPattern: try fetch()))))
        case 0xC3 where mandatory == nil: rip = try pop()
        case 0xC9 where mandatory == nil: // LEAVE
            registers[4] = registers[5]
            registers[5] = try pop()
        case 0xE8 where mandatory == nil:
            let offset = Int64(Int32(bitPattern: try fetch32()))
            try push(rip)
            branch(relative: offset)
        case 0xEB where mandatory == nil:
            branch(relative: Int64(Int8(bitPattern: try fetch())))
        case 0xE9 where mandatory == nil:
            branch(relative: Int64(Int32(bitPattern: try fetch32())))
        case 0x70...0x7F where mandatory == nil:
            let offset = Int64(Int8(bitPattern: try fetch()))
            if condition(opcode) { branch(relative: offset) }
        case 0xB8...0xBF where rexW && mandatory == nil,
             0x05 where rexW && mandatory == nil,
             0x2D where rexW && mandatory == nil,
             0x3D where rexW && mandatory == nil,
             0x89 where rexW && mandatory == nil,
             0x8B where rexW && mandatory == nil,
             0x8D where rexW && mandatory == nil,
             0x01 where rexW && mandatory == nil,
             0x03 where rexW && mandatory == nil,
             0x29 where rexW && mandatory == nil,
             0x2B where rexW && mandatory == nil,
             0x39 where rexW && mandatory == nil,
             0x3B where rexW && mandatory == nil,
             0x31 where rexW && mandatory == nil,
             0x33 where rexW && mandatory == nil,
             0x85 where rexW && mandatory == nil,
             0x81 where rexW && mandatory == nil,
             0x83 where rexW && mandatory == nil,
             0xC7 where rexW && mandatory == nil,
             0xF7 where rexW && mandatory == nil,
             0xFF where rexW && mandatory == nil:
            try executeInteger(second: opcode, rex: rex)
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
