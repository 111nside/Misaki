import Foundation

/// Limited ELF64 SHT_RELA/R_X86_64_RELATIVE parsing and relocation.
/// No PS4 SELF/PRX, symbol-based relocations or system module loading.
enum GuestRelocationError: Error, Equatable, LocalizedError {
    case invalidELF
    case invalidSectionTable
    case invalidRelocationTable
    case unsupportedRelocation(UInt32)
    case invalidAddress

    var errorDescription: String? {
        switch self {
        case .invalidELF: return "Expected little-endian ELF64 data."
        case .invalidSectionTable: return "Invalid or truncated ELF section headers."
        case .invalidRelocationTable: return "Invalid or truncated ELF RELA table."
        case .unsupportedRelocation(let value): return "Unsupported ELF relocation type \(value)."
        case .invalidAddress: return "Guest relocation address overflows."
        }
    }
}

struct GuestRelativeRelocation: Equatable {
    let offset: UInt64
    let addend: Int64
}

enum GuestRELARelocator {
    /// Parse all SHT_RELA sections. Only R_X86_64_RELATIVE (type 8, symbol index 0)
    /// is supported. A cap prevents malformed files from consuming unbounded memory.
    static func parse(_ data: Data) throws -> [GuestRelativeRelocation] {
        guard data.count >= 64, data.count <= 16 * 1024 * 1024 else {
            throw GuestRelocationError.invalidELF
        }
        let bytes = [UInt8](data)
        guard bytes[0...5].elementsEqual([0x7f, 0x45, 0x4c, 0x46, 2, 1]) else {
            throw GuestRelocationError.invalidELF
        }

        func read16(_ offset: Int) -> UInt16 {
            UInt16(bytes[offset]) | (UInt16(bytes[offset + 1]) << 8)
        }
        func read32(_ offset: Int) -> UInt32 {
            (0..<4).reduce(UInt32(0)) { $0 | (UInt32(bytes[offset + $1]) << ($1 * 8)) }
        }
        func read64(_ offset: Int) -> UInt64 {
            (0..<8).reduce(UInt64(0)) { $0 | (UInt64(bytes[offset + $1]) << ($1 * 8)) }
        }

        let tableOffset = read64(40)
        let count = Int(read16(60))
        let entrySize = Int(read16(58))
        // A missing table is legal for simple test programs.
        if tableOffset == 0 && count == 0 { return [] }
        guard count > 0, count <= 128, entrySize >= 64, entrySize <= 4096,
              tableOffset <= UInt64(bytes.count) else {
            throw GuestRelocationError.invalidSectionTable
        }
        let start = Int(tableOffset)
        guard count <= (bytes.count - start) / entrySize else {
            throw GuestRelocationError.invalidSectionTable
        }
        var relocations: [GuestRelativeRelocation] = []
        for i in 0..<count {
            let section = start + i * entrySize
            guard read32(section + 4) == 4 else { continue } // SHT_RELA
            let offset = read64(section + 24)
            let size = read64(section + 32)
            let stride = read64(section + 56)
            guard stride == 24, offset <= UInt64(bytes.count),
                  size <= UInt64(bytes.count) - offset, size % stride == 0,
                  size / stride <= 4096,
                  relocations.count + Int(size / stride) <= 4096 else {
                throw GuestRelocationError.invalidRelocationTable
            }
            for j in 0..<Int(size / stride) {
                let at = Int(offset) + j * 24
                let rOffset = read64(at)
                let rInfo = read64(at + 8)
                let rType = UInt32(truncatingIfNeeded: rInfo)
                guard rType == 8, rInfo >> 32 == 0 else {
                    throw GuestRelocationError.unsupportedRelocation(rType)
                }
                relocations.append(GuestRelativeRelocation(offset: rOffset,
                                                           addend: Int64(bitPattern: read64(at + 16))))
            }
        }
        return relocations
    }

    private static func addSigned(_ base: UInt64, _ delta: Int64) throws -> UInt64 {
        if delta >= 0 {
            let amount = UInt64(delta)
            guard base <= UInt64.max - amount else { throw GuestRelocationError.invalidAddress }
            return base + amount
        }
        let amount = delta.magnitude // handles Int64.min safely
        guard base >= amount else { throw GuestRelocationError.invalidAddress }
        return base - amount
    }

    /// Applies relative pointers after segments are mapped RW. Call protect() to
    /// restore final segment permissions. Other relocation types are rejected.
    @discardableResult
    static func apply(_ data: Data, loadBias: UInt64,
                      memory: inout VirtualMemory) throws -> Int {
        let relocations = try parse(data)
        var copy = memory
        for item in relocations {
            guard loadBias <= UInt64.max - item.offset else {
                throw GuestRelocationError.invalidAddress
            }
            let target = loadBias + item.offset
            let value = try addSigned(loadBias, item.addend)
            try copy.write64(target, value: value)
        }
        memory = copy
        return relocations.count
    }
}
