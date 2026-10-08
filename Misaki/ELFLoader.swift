import Foundation

/// ELF64 userspace *prototype* loader for small, unencrypted, independently
/// authored x86-64 test executables. Not a PS4 SELF/PRX loader.
enum ELFLoadError: Error, LocalizedError, Equatable {
    case unsupportedArchitecture
    case unsupportedType
    case invalidProgramHeaders
    case invalidSegment
    case imageTooLarge
    case entryPointUnmapped
    case noLoadableSegments

    var errorDescription: String? {
        switch self {
        case .unsupportedArchitecture: return "Only x86-64 ELF64 test executables are supported."
        case .unsupportedType: return "Only ET_EXEC test executables are supported."
        case .invalidProgramHeaders: return "The ELF program header table is invalid or truncated."
        case .invalidSegment: return "A loadable ELF segment has invalid bounds or overlaps another segment."
        case .imageTooLarge: return "The test ELF exceeds Misaki's prototype memory safety limits."
        case .entryPointUnmapped: return "The ELF entry point is not inside a loaded segment."
        case .noLoadableSegments: return "The ELF contains no loadable segments."
        }
    }
}

struct ELFLoadResult {
    var memory: VirtualMemory
    let entryPoint: UInt64
    let loadedSegments: Int
}

enum ELFLoader {
    /// We purposely limit allocations; this is not a representation of PS4 RAM.
    private static let maximumFileSize = 16 * 1024 * 1024
    private static let maximumSegmentSize: UInt64 = 8 * 1024 * 1024
    private static let maximumMappedBytes: UInt64 = 32 * 1024 * 1024

    static func load(_ data: Data) throws -> ELFLoadResult {
        guard data.count <= maximumFileSize else { throw ELFLoadError.imageTooLarge }
        let header = try ELFInspector.inspect(data)
        guard header.machine == 0x3E else { throw ELFLoadError.unsupportedArchitecture }
        guard header.objectType == 2 else { throw ELFLoadError.unsupportedType }
        let bytes = [UInt8](data)

        func read16(_ offset: Int) -> UInt16 {
            UInt16(bytes[offset]) | (UInt16(bytes[offset + 1]) << 8)
        }
        func read32(_ offset: Int) -> UInt32 {
            (0..<4).reduce(UInt32(0)) { $0 | (UInt32(bytes[offset + $1]) << ($1 * 8)) }
        }
        func read64(_ offset: Int) -> UInt64 {
            (0..<8).reduce(UInt64(0)) { $0 | (UInt64(bytes[offset + $1]) << ($1 * 8)) }
        }

        // ELF64 program headers are at least 56 bytes each.
        let count = Int(header.programHeaderCount)
        let entrySize = Int(read16(54))
        guard count > 0, count <= 64, entrySize >= 56, entrySize <= 4096,
              header.programHeaderOffset <= UInt64(bytes.count) else {
            throw ELFLoadError.invalidProgramHeaders
        }
        let tableOffset = Int(header.programHeaderOffset)
        guard count <= (bytes.count - tableOffset) / entrySize else {
            throw ELFLoadError.invalidProgramHeaders
        }

        var memory = VirtualMemory()
        var regions: [(start: UInt64, end: UInt64)] = []
        var totalMapped: UInt64 = 0
        var loadedSegments = 0

        for index in 0..<count {
            let offset = tableOffset + index * entrySize
            guard read32(offset) == 1 else { continue } // PT_LOAD
            let fileOffset = read64(offset + 8)
            let virtualAddress = read64(offset + 16)
            let fileSize = read64(offset + 32)
            let memorySize = read64(offset + 40)
            guard memorySize >= fileSize,
                  memorySize <= maximumSegmentSize,
                  totalMapped <= maximumMappedBytes - memorySize,
                  fileOffset <= UInt64(bytes.count),
                  fileSize <= UInt64(bytes.count) - fileOffset,
                  virtualAddress <= UInt64.max - memorySize else {
                throw ELFLoadError.invalidSegment
            }
            if memorySize == 0 { continue }
            let end = virtualAddress + memorySize // exclusive
            for region in regions {
                guard end <= region.start || virtualAddress >= region.end else {
                    throw ELFLoadError.invalidSegment
                }
            }
            regions.append((start: virtualAddress, end: end))
            totalMapped += memorySize
            let start = Int(fileOffset)
            let limit = start + Int(fileSize)
            var segment = Array(bytes[start..<limit])
            segment.append(contentsOf: repeatElement(UInt8(0), count: Int(memorySize - fileSize)))
            memory.load(segment, at: virtualAddress)
            loadedSegments += 1
        }
        guard loadedSegments > 0 else { throw ELFLoadError.noLoadableSegments }
        guard regions.contains(where: { $0.start <= header.entryPoint && header.entryPoint < $0.end }) else {
            throw ELFLoadError.entryPointUnmapped
        }
        return ELFLoadResult(memory: memory,
                             entryPoint: header.entryPoint,
                             loadedSegments: loadedSegments)
    }
}
