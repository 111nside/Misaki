import Foundation

enum EmulatorError: Error, Equatable {
    case unmappedAddress(UInt64)
    case invalidInstruction(UInt8)
    case unsupportedSyscall(UInt64)
    case invalidSyscallArguments
    case halted
    case stepLimit
}

/// Sparse byte map. Segment permissions, page tables, MMU and ASLR are not modeled.
struct VirtualMemory {
    private var bytes: [UInt64: UInt8] = [:]
    mutating func load(_ data: [UInt8], at address: UInt64) {
        for (index, byte) in data.enumerated() { bytes[address &+ UInt64(index)] = byte }
    }
    func read8(_ address: UInt64) throws -> UInt8 {
        guard let byte = bytes[address] else { throw EmulatorError.unmappedAddress(address) }
        return byte
    }
    mutating func write8(_ address: UInt64, value: UInt8) throws {
        guard bytes[address] != nil else { throw EmulatorError.unmappedAddress(address) }
        bytes[address] = value
    }
}
