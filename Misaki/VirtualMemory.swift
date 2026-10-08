import Foundation

enum EmulatorError: Error, Equatable {
    case unmappedAddress(UInt64)
    case protectionFault(UInt64)
    case overlappingMapping
    case invalidMemoryMapping
    case invalidInstruction(UInt8)
    case unsupportedSyscall(UInt64)
    case invalidSyscallArguments
    case divideError
    case halted
    case stepLimit
}

struct MemoryPermissions: OptionSet, Equatable {
    let rawValue: UInt8
    static let read = MemoryPermissions(rawValue: 1)
    static let write = MemoryPermissions(rawValue: 2)
    static let execute = MemoryPermissions(rawValue: 4)
    static let all: MemoryPermissions = [.read, .write, .execute]
}

/// Sparse byte-backed guest virtual memory with region-level R/W/X checks.
/// This is NOT a PS4 MMU, page table, ASLR implementation or physical RAM model.
struct VirtualMemory {
    private struct Region {
        let start: UInt64
        let end: UInt64   // exclusive
        let permissions: MemoryPermissions
    }

    private var bytes: [UInt64: UInt8] = [:]
    private var regions: [Region] = []

    mutating func map(_ data: [UInt8], at address: UInt64,
                      permissions: MemoryPermissions) throws {
        guard !data.isEmpty,
              address <= UInt64.max - UInt64(data.count) else {
            throw EmulatorError.invalidMemoryMapping
        }
        let end = address + UInt64(data.count)
        guard !regions.contains(where: { end > $0.start && address < $0.end }) else {
            throw EmulatorError.overlappingMapping
        }
        for (index, byte) in data.enumerated() {
            bytes[address + UInt64(index)] = byte
        }
        regions.append(Region(start: address, end: end, permissions: permissions))
    }

    /// Legacy helper for raw test programs. ELF segments use map(_:at:permissions:).
    mutating func load(_ data: [UInt8], at address: UInt64) {
        // Existing callers use a fresh memory instance and a small, fixed address.
        // Treat invalid/overlapping test mappings as a failed mapping, never wrap addresses.
        try? map(data, at: address, permissions: .all)
    }

    private func checked(_ address: UInt64, for permission: MemoryPermissions) throws -> UInt8 {
        guard let byte = bytes[address],
              let region = regions.first(where: { $0.start <= address && address < $0.end }) else {
            throw EmulatorError.unmappedAddress(address)
        }
        guard region.permissions.contains(permission) else {
            throw EmulatorError.protectionFault(address)
        }
        return byte
    }

    func read8(_ address: UInt64) throws -> UInt8 {
        try checked(address, for: .read)
    }

    func fetch8(_ address: UInt64) throws -> UInt8 {
        try checked(address, for: .execute)
    }

    mutating func write8(_ address: UInt64, value: UInt8) throws {
        _ = try checked(address, for: .write)
        bytes[address] = value
    }

    /// Validate the whole write before changing any byte (atomic with respect
    /// to guest protection faults; the interpreter itself is single-threaded).
    mutating func writeBytes(_ address: UInt64, values: [UInt8]) throws {
        guard !values.isEmpty, address <= UInt64.max - UInt64(values.count - 1) else {
            throw EmulatorError.invalidMemoryMapping
        }
        for i in values.indices {
            _ = try checked(address + UInt64(i), for: .write)
        }
        for i in values.indices {
            bytes[address + UInt64(i)] = values[i]
        }
    }

    func read64(_ address: UInt64) throws -> UInt64 {
        guard address <= UInt64.max - 7 else { throw EmulatorError.invalidMemoryMapping }
        var value: UInt64 = 0
        for i in 0..<8 { value |= UInt64(try read8(address + UInt64(i))) << (i * 8) }
        return value
    }

    mutating func write64(_ address: UInt64, value: UInt64) throws {
        guard address <= UInt64.max - 7 else { throw EmulatorError.invalidMemoryMapping }
        // Validate the entire write first so an invalid address cannot partially modify memory.
        for i in 0..<8 { _ = try checked(address + UInt64(i), for: .write) }
        for i in 0..<8 {
            bytes[address + UInt64(i)] = UInt8(truncatingIfNeeded: value >> (i * 8))
        }
    }
}
