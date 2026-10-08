import Foundation

/// Independently authored ELF64 ET_DYN sample, without Sony content.
/// Code at vaddr 0x1000 CALLs an imported function through a GOT entry at 0x2000.
/// DT_RELA also initializes a relative pointer at 0x2008 to vaddr 0x1000.
enum DemoDynamicELF {
    static let name = "libMisakiDemo"
    static let importSymbol = "misakiReturn42"
    static let localSymbol = "localEntry42"
    static let defaultBias: UInt64 = 0x4000
    static let functionAddress: UInt64 = 0x9000

    static let externalFunction: [UInt8] = [
        0x48, 0xB8, 40, 0, 0, 0, 0, 0, 0, 0, // MOV RAX, 40
        0x48, 0x05, 2, 0, 0, 0,                 // ADD RAX, 2
        0xC3                                     // RET
    ]

    static func libraries() throws -> GuestDynamicLinker {
        var linker = GuestDynamicLinker()
        try linker.register(GuestLibrary(name: name, exports: [importSymbol: functionAddress]))
        return linker
    }

    static func make() -> Data {
        var b = [UInt8](repeating: 0, count: 0x500)
        func put(_ x: UInt64, _ p: Int, _ n: Int) {
            for i in 0..<n { b[p + i] = UInt8(truncatingIfNeeded: x >> (i * 8)) }
        }
        b[0] = 0x7f; b[1] = 0x45; b[2] = 0x4c; b[3] = 0x46
        b[4] = 2; b[5] = 1; b[6] = 1
        put(3, 16, 2) // ET_DYN
        put(0x3e, 18, 2) // x86-64
        put(1, 20, 4)
        put(0x1000, 24, 8) // e_entry
        put(64, 32, 8) // phoff
        put(64, 52, 2); put(56, 54, 2); put(3, 56, 2)
        // RX PT_LOAD (0x200 bytes in file? 0x100 from 0x200)
        put(1, 64, 4); put(5, 68, 4)
        put(0x200, 72, 8); put(0x1000, 80, 8)
        put(0x100, 96, 8); put(0x100, 104, 8)
        put(0x100, 112, 8)
        // RW PT_LOAD with GOT, .dynamic, dynstr, dynsym, hash, RELA
        let p2 = 64 + 56
        put(1, p2, 4); put(6, p2 + 4, 4)
        put(0x300, p2 + 8, 8); put(0x2000, p2 + 16, 8)
        put(0x200, p2 + 32, 8); put(0x200, p2 + 40, 8)
        put(0x100, p2 + 48, 8)
        // PT_DYNAMIC inside RW PT_LOAD
        let p3 = 64 + 112
        put(2, p3, 4); put(6, p3 + 4, 4)
        put(0x310, p3 + 8, 8); put(0x2010, p3 + 16, 8)
        put(13 * 16, p3 + 32, 8); put(13 * 16, p3 + 40, 8)
        put(8, p3 + 48, 8)

        // CALL qword [RIP + 0xFF9] (vaddr=0x2000); HLT.
        let code: [UInt8] = [0x48, 0xFF, 0x15, 0xF9, 0x0F, 0, 0, 0xF4]
        b.replaceSubrange(0x200..<(0x200 + code.count), with: code)
        // Defined local export at vaddr=0x1080: MOV RAX,42 ; RET.
        let localCode: [UInt8] = [0x48, 0xB8, 42, 0, 0, 0, 0, 0, 0, 0, 0xC3]
        b.replaceSubrange(0x280..<(0x280 + localCode.count), with: localCode)

        let str = Array(("\0" + importSymbol + "\0" + name + "\0" + localSymbol + "\0").utf8)
        let libraryOffset = 1 + importSymbol.utf8.count + 1
        let localOffset = libraryOffset + name.utf8.count + 1
        b.replaceSubrange(0x400..<(0x400 + str.count), with: str)

        let dynamic: [(UInt64, UInt64)] = [
            (1, UInt64(libraryOffset)), // DT_NEEDED
            (5, 0x2100), (10, UInt64(str.count)), // STRTAB / STRSZ
            (6, 0x2140), (11, 24), // SYMTAB / SYMENT
            (4, 0x2190), // SysV DT_HASH
            (7, 0x21c0), (8, 24), (9, 24), // RELA / RELASZ / RELAENT
            (23, 0x21d8), (2, 24), (20, 7), // JMPREL / PLTRELSZ / PLTREL=RELA
            (0, 0)
        ]
        for (i, item) in dynamic.enumerated() {
            put(item.0, 0x310 + i * 16, 8)
            put(item.1, 0x318 + i * 16, 8)
        }
        // Elf64_Sym index 1: undefined imported function.
        put(1, 0x440 + 24, 4) // name
        put(0x12, 0x440 + 28, 1) // GLOBAL FUNC
        // Elf64_Sym index 2: defined local export.
        put(UInt64(localOffset), 0x440 + 48, 4)
        put(0x12, 0x440 + 52, 1)
        put(1, 0x440 + 54, 2) // non-SHN_UNDEF
        put(0x1080, 0x440 + 56, 8)
        put(UInt64(localCode.count), 0x440 + 64, 8)
        // SysV hash: nbucket=1, nchain=3
        put(1, 0x490, 4); put(3, 0x494, 4)
        put(1, 0x498, 4)
        // RELA at 0x4c0, R_X86_64_RELATIVE: *(bias+0x2008) = bias+0x1000
        put(0x2008, 0x4c0, 8); put(8, 0x4c8, 8); put(0x1000, 0x4d0, 8)
        // PLT RELA at 0x4d8: *(bias+0x2000) = imported guest symbol
        put(0x2000, 0x4d8, 8); put((1 << 32) | 7, 0x4e0, 8)
        return Data(b)
    }
}

struct DemoDynamicResult {
    let output: UInt64
    let instructions: Int
    let imports: [String]
    let relocations: Int
    let needed: [String]
    let guestPointer: UInt64
    let stackRestored: Bool
    let halted: Bool
    var passed: Bool {
        output == 42 && instructions == 5 && imports == [DemoDynamicELF.importSymbol] &&
        relocations == 2 && needed == [DemoDynamicELF.name] &&
        guestPointer == DemoDynamicELF.defaultBias + 0x1000 && stackRestored && halted
    }
}

enum DemoDynamicRunner {
    static func run() throws -> DemoDynamicResult {
        var cpu = X86Interpreter()
        let module = try cpu.loadDynamicELF(DemoDynamicELF.make(),
                                            loadBias: DemoDynamicELF.defaultBias,
                                            libraries: DemoDynamicELF.libraries())
        try cpu.memory.map(DemoDynamicELF.externalFunction,
                           at: DemoDynamicELF.functionAddress, permissions: [.read, .execute])
        let startStack = cpu.rsp
        try cpu.run(maxSteps: 20)
        return DemoDynamicResult(output: cpu.rax,
                                 instructions: cpu.executedInstructions,
                                 imports: module.boundImports,
                                 relocations: module.relocationCount,
                                 needed: module.neededLibraries,
                                 guestPointer: try cpu.memory.read64(0x6008),
                                 stackRestored: cpu.rsp == startStack, halted: cpu.isHalted)
    }
}
