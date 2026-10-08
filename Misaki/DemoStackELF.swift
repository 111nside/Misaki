import Foundation

/// Small self-authored executable exercising CALL/RET and PUSH/POP.
/// mov rax, 5; call helper; hlt; helper: push rax; add rax, 3; pop rcx; ret
/// Expected: RAX=8, RCX=5, stack pointer restored.
enum DemoStackELF {
    static func make() -> Data {
        let code: [UInt8] = [
            0x48, 0xB8, 5, 0, 0, 0, 0, 0, 0, 0,
            0xE8, 1, 0, 0, 0,
            0xF4,
            0x50,
            0x48, 0x05, 3, 0, 0, 0,
            0x59,
            0xC3
        ]
        var data = DemoELF.make()
        data.replaceSubrange(0x100..<data.count, with: code)
        // Existing demo has a single PT_LOAD segment at offset 0x100.
        // Update p_filesz and p_memsz; keep 16 bytes of zero-filled BSS.
        for index in 0..<8 {
            data[96 + index] = UInt8(truncatingIfNeeded: UInt64(code.count) >> (index * 8))
            data[104 + index] = UInt8(truncatingIfNeeded: UInt64(code.count + 16) >> (index * 8))
        }
        return data
    }
}
