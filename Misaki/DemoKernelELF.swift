import Foundation

/// Self-authored ELF64 fixture that calls Misaki's demonstration ABI.
/// It is NOT a PS4 system executable, firmware component, or PS4 kernel.
enum DemoKernelELF {
    static let expectedOutput = "Misaki!\n"

    static func make() -> Data {
        // mov rax, 1; mov rdi, 0x1100; mov rsi, 8; syscall;
        // mov rax, 60; xor rdi, rdi; syscall.
        let code: [UInt8] = [
            0x48, 0xB8, 1, 0, 0, 0, 0, 0, 0, 0,
            0x48, 0xBF, 0, 0x11, 0, 0, 0, 0, 0, 0,
            0x48, 0xBE, 8, 0, 0, 0, 0, 0, 0, 0,
            0x0F, 0x05,
            0x48, 0xB8, 60, 0, 0, 0, 0, 0, 0, 0,
            0x48, 0x31, 0xFF,
            0x0F, 0x05
        ]
        let message = Array(expectedOutput.utf8)
        let segmentFileSize = 0x100 + message.count
        var bytes = [UInt8](repeating: 0, count: 0x100 + segmentFileSize)

        func writeLE(_ value: UInt64, at offset: Int, width: Int) {
            for i in 0..<width {
                bytes[offset + i] = UInt8(truncatingIfNeeded: value >> (i * 8))
            }
        }
        bytes[0] = 0x7F
        bytes[1] = 0x45; bytes[2] = 0x4C; bytes[3] = 0x46
        bytes[4] = 2; bytes[5] = 1; bytes[6] = 1
        writeLE(2, at: 16, width: 2) // ET_EXEC
        writeLE(0x3E, at: 18, width: 2) // EM_X86_64
        writeLE(1, at: 20, width: 4)
        writeLE(0x1000, at: 24, width: 8)
        writeLE(64, at: 32, width: 8)
        writeLE(64, at: 52, width: 2)
        writeLE(56, at: 54, width: 2)
        writeLE(1, at: 56, width: 2)
        writeLE(1, at: 64, width: 4) // PT_LOAD
        writeLE(5, at: 68, width: 4) // R + X flags (not enforced yet)
        writeLE(0x100, at: 72, width: 8)
        writeLE(0x1000, at: 80, width: 8)
        writeLE(UInt64(segmentFileSize), at: 96, width: 8)
        writeLE(UInt64(segmentFileSize + 16), at: 104, width: 8)
        writeLE(0x100, at: 112, width: 8)
        bytes.replaceSubrange(0x100..<(0x100 + code.count), with: code)
        bytes.replaceSubrange(0x200..<(0x200 + message.count), with: message)
        return Data(bytes)
    }
}
