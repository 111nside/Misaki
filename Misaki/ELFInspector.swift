import Foundation

enum ELFInspectError: LocalizedError {
    case tooSmall, invalidMagic, unsupportedClass, unsupportedEndianness

    var errorDescription: String? {
        switch self {
        case .tooSmall: return "File is too small for an ELF64 header."
        case .invalidMagic: return "File does not have an ELF header."
        case .unsupportedClass: return "Only 64-bit ELF is supported."
        case .unsupportedEndianness: return "Only little-endian ELF is supported."
        }
    }
}

struct ELFHeader: Equatable {
    let objectType: UInt16
    let machine: UInt16
    let entryPoint: UInt64
    let programHeaderOffset: UInt64
    let programHeaderCount: UInt16

    var machineLabel: String {
        switch machine {
        case 0x3E: return "x86-64"
        case 0xB7: return "AArch64"
        default: return String(format: "Unknown (0x%04X)", machine)
        }
    }
}

enum ELFInspector {
    static func inspect(_ data: Data) throws -> ELFHeader {
        guard data.count >= 64 else { throw ELFInspectError.tooSmall }
        let b = [UInt8](data.prefix(64))
        guard b[0] == 0x7f && b[1] == 0x45 && b[2] == 0x4c && b[3] == 0x46 else {
            throw ELFInspectError.invalidMagic
        }
        guard b[4] == 2 else { throw ELFInspectError.unsupportedClass }
        guard b[5] == 1 else { throw ELFInspectError.unsupportedEndianness }

        func r16(_ o: Int) -> UInt16 {
            UInt16(b[o]) | (UInt16(b[o + 1]) << 8)
        }
        func r64(_ o: Int) -> UInt64 {
            (0..<8).reduce(UInt64(0)) { $0 | (UInt64(b[o + $1]) << ($1 * 8)) }
        }
        return ELFHeader(objectType: r16(16), machine: r16(18), entryPoint: r64(24),
                         programHeaderOffset: r64(32), programHeaderCount: r16(56))
    }
}
