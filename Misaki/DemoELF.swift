import Foundation

/// A tiny, self-authored ELF64 demo that runs only MOV, ADD, and HLT.
/// This is a diagnostic fixture, not PlayStation firmware or a PS4 executable.
enum DemoELF {
    static func make() -> Data {
        // mov rax, 5; add rax, 3; hlt
        let code: [UInt8] = [
            0x48, 0xB8, 5, 0, 0, 0, 0, 0, 0, 0,
            0x48, 0x05, 3, 0, 0, 0,
            0xF4
        ]
        var bytes = [UInt8](repeating: 0, count: 0x100 + code.count)
        func put(_ value: UInt64, at offset: Int, bytes width: Int) {
            for i in 0..<width {
                bytes[offset + i] = UInt8(truncatingIfNeeded: value >> (i * 8))
            }
        }
        bytes[0] = 0x7F
        bytes[1] = 0x45; bytes[2] = 0x4C; bytes[3] = 0x46
        bytes[4] = 2  // ELF64
        bytes[5] = 1  // little-endian
        bytes[6] = 1  // ELF version
        put(2, at: 16, bytes: 2)       // ET_EXEC
        put(0x3E, at: 18, bytes: 2)    // EM_X86_64
        put(1, at: 20, bytes: 4)       // EV_CURRENT
        put(0x1000, at: 24, bytes: 8)  // entry
        put(64, at: 32, bytes: 8)      // program header table offset
        put(64, at: 52, bytes: 2)      // ELF header size
        put(56, at: 54, bytes: 2)      // program header entry size
        put(1, at: 56, bytes: 2)       // count
        // Program header (PT_LOAD, readable/executable)
        put(1, at: 64, bytes: 4)               // PT_LOAD
        put(5, at: 68, bytes: 4)               // PF_R | PF_X
        put(0x100, at: 72, bytes: 8)           // p_offset
        put(0x1000, at: 80, bytes: 8)          // p_vaddr
        put(UInt64(code.count), at: 96, bytes: 8) // p_filesz
        put(UInt64(code.count + 16), at: 104, bytes: 8) // p_memsz; 16 BSS bytes
        put(0x100, at: 112, bytes: 8)          // p_align
        bytes.replaceSubrange(0x100..<(0x100 + code.count), with: code)
        return Data(bytes)
    }
}
