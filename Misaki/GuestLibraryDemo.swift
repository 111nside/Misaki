import Foundation

struct LinkedDemoResult {
    let value: UInt64
    let instructions: Int
    let linkedImports: Int
    let libraryAddress: UInt64
    let stackRestored: Bool
    let halted: Bool

    var passed: Bool {
        value == 42 && instructions == 5 && linkedImports == 1 &&
        libraryAddress == 0x3000 && stackRestored && halted
    }
}

/// A synthetic ELF64 executable with two PT_LOAD segments: RX code and RW GOT.
/// This is self-authored test data, not a PS4 SELF, PRX, or firmware file.
enum GuestLinkedELF {
    static func make() -> Data {
        let code: [UInt8] = [0x48, 0xFF, 0x15, 0xF9, 0, 0, 0, 0xF4]
        var bytes = [UInt8](repeating: 0, count: 0x208)
        func put(_ number: UInt64, at offset: Int, width: Int) {
            for i in 0..<width {
                bytes[offset + i] = UInt8(truncatingIfNeeded: number >> (i * 8))
            }
        }
        bytes[0] = 0x7f; bytes[1] = 0x45; bytes[2] = 0x4c; bytes[3] = 0x46
        bytes[4] = 2; bytes[5] = 1; bytes[6] = 1
        put(2, at: 16, width: 2) // ET_EXEC
        put(0x3e, at: 18, width: 2) // x86-64
        put(1, at: 20, width: 4)
        put(0x1000, at: 24, width: 8) // e_entry
        put(64, at: 32, width: 8) // e_phoff
        put(64, at: 52, width: 2)
        put(56, at: 54, width: 2)
        put(2, at: 56, width: 2)
        // PT_LOAD #1: code, PF_R | PF_X
        put(1, at: 64, width: 4)
        put(5, at: 68, width: 4)
        put(0x100, at: 72, width: 8)
        put(0x1000, at: 80, width: 8)
        put(UInt64(code.count), at: 96, width: 8)
        put(UInt64(code.count), at: 104, width: 8)
        put(0x100, at: 112, width: 8)
        // PT_LOAD #2: 8-byte GOT slot, PF_R | PF_W
        let second = 64 + 56
        put(1, at: second, width: 4)
        put(6, at: second + 4, width: 4)
        put(0x200, at: second + 8, width: 8)
        put(0x1100, at: second + 16, width: 8)
        put(8, at: second + 32, width: 8)
        put(8, at: second + 40, width: 8)
        put(0x100, at: second + 48, width: 8)
        bytes.replaceSubrange(0x100..<(0x100 + code.count), with: code)
        return Data(bytes)
    }
}

/// Runs a tiny ELF application that CALLs a function in a separately mapped
/// guest code library through an import pointer patched by GuestDynamicLinker.
/// This is self-authored test code, not Sony firmware or an actual PS4 library.
enum GuestLibraryDemo {
    static let libraryName = "libMisakiDemo"
    static let symbolName = "misakiReturn42"

    static func preparedCPU() throws -> (cpu: X86Interpreter, report: GuestLinkReport) {
        // CALL qword ptr [RIP + 0xF9] ; HLT
        // Instruction ends at 0x1007, so 0x1007+0xF9 = 0x1100.
        // ELFLoader maps executable code RX and an 8-byte GOT slot RW.
        var cpu = X86Interpreter()
        try cpu.loadELF(GuestLinkedELF.make())

        // Guest library function: mov rax,40; add rax,2; ret.
        let function: [UInt8] = [
            0x48, 0xB8, 40, 0, 0, 0, 0, 0, 0, 0,
            0x48, 0x05, 2, 0, 0, 0,
            0xC3
        ]
        try cpu.memory.map(function, at: 0x3000, permissions: [.read, .execute])

        var linker = GuestDynamicLinker()
        try linker.register(GuestLibrary(name: libraryName,
                                         exports: [symbolName: 0x3000]))
        let report = try linker.link([
            GuestImport(library: libraryName, symbol: symbolName, slotAddress: 0x1100)
        ], into: &cpu.memory)
        // A GOT entry becomes read-only after successful linking.
        try cpu.memory.protect(at: 0x1100, size: 8, permissions: [.read])
        return (cpu, report)
    }

    static func run() throws -> LinkedDemoResult {
        var prepared = try preparedCPU()
        let initialSP = prepared.cpu.rsp
        try prepared.cpu.run(maxSteps: 20)
        return LinkedDemoResult(
            value: prepared.cpu.rax,
            instructions: prepared.cpu.executedInstructions,
            linkedImports: prepared.report.count,
            libraryAddress: try prepared.cpu.memory.read64(0x1100),
            stackRestored: prepared.cpu.rsp == initialSP,
            halted: prepared.cpu.isHalted
        )
    }
}
