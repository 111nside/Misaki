import Foundation

/// An ELF64 ET_DYN loader for original, unencrypted x86-64 test binaries.
/// No PlayStation SELF/PRX/NID/firmware compatibility or host code execution.
enum GuestDynamicELFError: Error, LocalizedError, Equatable {
    case unsupportedImage
    case malformedProgramHeaders
    case malformedDynamicTable
    case malformedSymbolTable
    case malformedRelocationTable
    case invalidImageAddress
    case missingLibrary(String)
    case unresolvedSymbol(String)
    case unsupportedRelocation(UInt32)
    case imageTooLarge

    var errorDescription: String? {
        switch self {
        case .unsupportedImage: return "Expected an unencrypted x86-64 ELF64 ET_DYN test module."
        case .malformedProgramHeaders: return "Invalid dynamic ELF program header or segment."
        case .malformedDynamicTable: return "Invalid or incomplete ELF PT_DYNAMIC table."
        case .malformedSymbolTable: return "Invalid ELF dynamic symbol or string table."
        case .malformedRelocationTable: return "Invalid or truncated ELF dynamic RELA table."
        case .invalidImageAddress: return "Guest image address overflow or unmapped address."
        case .missingLibrary(let name): return "Missing guest library: \(name)"
        case .unresolvedSymbol(let name): return "Unresolved guest symbol: \(name)"
        case .unsupportedRelocation(let type): return "Unsupported ELF relocation type: \(type)"
        case .imageTooLarge: return "Dynamic ELF exceeds the prototype's safety limits."
        }
    }
}

struct GuestDynamicELFImage {
    let memory: VirtualMemory
    let entryPoint: UInt64
    let loadBias: UInt64
    let neededLibraries: [String]
    let exports: [String: UInt64]
    let boundImports: [String]
    let relocationCount: Int
}

private struct GuestELFSegment {
    let address: UInt64
    let size: Int
    let permissions: MemoryPermissions
}

private struct GuestELFSymbol {
    let name: String
    let defined: Bool
    let value: UInt64
    let binding: UInt8
}

/// Deliberately bounded implementation of PT_LOAD + PT_DYNAMIC, SysV DT_HASH,
/// DT_NEEDED / DT_SYMTAB / DT_STRTAB, and RELA relocations:
/// R_X86_64_RELATIVE (8), GLOB_DAT (6), JUMP_SLOT (7).
/// GNU hash, TLS, copy relocations, versioning and ELF dependency recursion are unsupported.
enum GuestDynamicELFLoader {
    private static let maxFileBytes = 16 * 1024 * 1024
    private static let maxSegmentBytes: UInt64 = 8 * 1024 * 1024
    private static let maxMappedBytes: UInt64 = 32 * 1024 * 1024
    private static let maxSymbolCount = 512
    private static let maxRelocations = 1024

    private static func add(_ lhs: UInt64, _ rhs: UInt64) throws -> UInt64 {
        guard lhs <= UInt64.max - rhs else { throw GuestDynamicELFError.invalidImageAddress }
        return lhs + rhs
    }
    private static func addSigned(_ lhs: UInt64, _ delta: Int64) throws -> UInt64 {
        if delta >= 0 { return try add(lhs, UInt64(delta)) }
        guard lhs >= delta.magnitude else { throw GuestDynamicELFError.invalidImageAddress }
        return lhs - delta.magnitude
    }
    private static func uint16(_ bytes: [UInt8], _ p: Int) -> UInt16 {
        UInt16(bytes[p]) | UInt16(bytes[p + 1]) << 8
    }
    private static func uint32(_ bytes: [UInt8], _ p: Int) -> UInt32 {
        (0..<4).reduce(UInt32(0)) { $0 | (UInt32(bytes[p + $1]) << ($1 * 8)) }
    }
    private static func uint64(_ bytes: [UInt8], _ p: Int) -> UInt64 {
        (0..<8).reduce(UInt64(0)) { $0 | (UInt64(bytes[p + $1]) << ($1 * 8)) }
    }
    private static func guestNumber(_ mem: VirtualMemory, _ address: UInt64, count: Int) throws -> UInt64 {
        guard count == 4 || count == 8, address <= UInt64.max - UInt64(count - 1) else {
            throw GuestDynamicELFError.invalidImageAddress
        }
        var result: UInt64 = 0
        for n in 0..<count { result |= UInt64(try mem.read8(address + UInt64(n))) << (8 * n) }
        return result
    }
    private static func guestString(_ mem: VirtualMemory, _ start: UInt64,
                                    offset: UInt32, size: UInt64) throws -> String {
        guard UInt64(offset) < size, size <= 65_536 else { throw GuestDynamicELFError.malformedSymbolTable }
        var bytes: [UInt8] = []
        let maxLength = min(size - UInt64(offset), 1024)
        for i in 0..<Int(maxLength) {
            let b = try mem.read8(try add(try add(start, UInt64(offset)), UInt64(i)))
            if b == 0 {
                guard let string = String(bytes: bytes, encoding: .utf8) else {
                    throw GuestDynamicELFError.malformedSymbolTable
                }
                return string
            }
            bytes.append(b)
        }
        throw GuestDynamicELFError.malformedSymbolTable
    }

    static func load(_ data: Data, loadBias: UInt64,
                     libraries: GuestDynamicLinker = GuestDynamicLinker()) throws -> GuestDynamicELFImage {
        guard data.count <= maxFileBytes else { throw GuestDynamicELFError.imageTooLarge }
        let elf = try ELFInspector.inspect(data)
        guard elf.machine == 0x3e, elf.objectType == 3 else { throw GuestDynamicELFError.unsupportedImage }
        let raw = [UInt8](data)
        let phSize = Int(uint16(raw, 54))
        let phCount = Int(elf.programHeaderCount)
        guard phCount > 0, phCount <= 64, phSize >= 56, phSize <= 4096,
              elf.programHeaderOffset <= UInt64(raw.count) else {
            throw GuestDynamicELFError.malformedProgramHeaders
        }
        let table = Int(elf.programHeaderOffset)
        guard phCount <= (raw.count - table) / phSize else {
            throw GuestDynamicELFError.malformedProgramHeaders
        }

        var memory = VirtualMemory()
        var segments: [GuestELFSegment] = []
        var mapped: UInt64 = 0
        var dynamicRange: (offset: Int, size: Int)?
        for i in 0..<phCount {
            let p = table + i * phSize
            let kind = uint32(raw, p)
            let fileOffset = uint64(raw, p + 8)
            let virtualAddress = uint64(raw, p + 16)
            let fileSize = uint64(raw, p + 32)
            let memorySize = uint64(raw, p + 40)
            guard fileOffset <= UInt64(raw.count), fileSize <= UInt64(raw.count) - fileOffset else {
                throw GuestDynamicELFError.malformedProgramHeaders
            }
            if kind == 2 { // PT_DYNAMIC, containing Elf64_Dyn entries
                guard dynamicRange == nil, fileSize >= 16, fileSize <= 4096, fileSize % 16 == 0 else {
                    throw GuestDynamicELFError.malformedDynamicTable
                }
                dynamicRange = (Int(fileOffset), Int(fileSize))
            }
            guard kind == 1 else { continue } // PT_LOAD
            guard memorySize >= fileSize, memorySize <= maxSegmentBytes,
                  mapped <= maxMappedBytes - memorySize else {
                throw GuestDynamicELFError.imageTooLarge
            }
            if memorySize == 0 { continue }
            let address = try add(loadBias, virtualAddress)
            _ = try add(address, memorySize)
            var contents = Array(raw[Int(fileOffset)..<(Int(fileOffset) + Int(fileSize))])
            contents.append(contentsOf: repeatElement(0, count: Int(memorySize - fileSize)))
            // Staging permissions allow relocation writes; final ELF flags are restored later.
            try memory.map(contents, at: address, permissions: .all)
            mapped += memorySize
            let flags = uint32(raw, p + 4)
            var perms: MemoryPermissions = []
            if flags & 4 != 0 { perms.insert(.read) }
            if flags & 2 != 0 { perms.insert(.write) }
            if flags & 1 != 0 { perms.insert(.execute) }
            segments.append(GuestELFSegment(address: address, size: Int(memorySize), permissions: perms))
        }
        guard !segments.isEmpty, let dynamicRange else { throw GuestDynamicELFError.malformedDynamicTable }
        let entry = try add(loadBias, elf.entryPoint)
        guard segments.contains(where: { $0.permissions.contains(.execute) && entry >= $0.address && entry - $0.address < UInt64($0.size) }) else {
            throw GuestDynamicELFError.invalidImageAddress
        }

        // Dynamic values that are addresses use the original ELF virtual address plus loadBias.
        var tags: [UInt64: [UInt64]] = [:]
        var terminated = false
        for p in stride(from: dynamicRange.offset, to: dynamicRange.offset + dynamicRange.size, by: 16) {
            let tag = uint64(raw, p)
            let value = uint64(raw, p + 8)
            if tag == 0 { terminated = true; break } // DT_NULL
            tags[tag, default: []].append(value)
        }
        guard terminated else { throw GuestDynamicELFError.malformedDynamicTable }
        func tag(_ kind: UInt64) throws -> UInt64? {
            guard let entries = tags[kind] else { return nil }
            guard entries.count == 1 else { throw GuestDynamicELFError.malformedDynamicTable }
            return entries[0]
        }
        let strVirtual = try tag(5)
        let strSize = try tag(10)
        let symVirtual = try tag(6)
        let symEnt = try tag(11)
        let hashVirtual = try tag(4)
        // The prototype requires SysV DT_HASH to bound symbol-table traversal.
        guard let strVirtual, let strSize, let symVirtual, let symEnt, let hashVirtual,
              symEnt == 24, strSize > 0, strSize <= 65_536 else {
            throw GuestDynamicELFError.malformedDynamicTable
        }
        let strAddress = try add(loadBias, strVirtual)
        let symAddress = try add(loadBias, symVirtual)
        let hashAddress = try add(loadBias, hashVirtual)
        let bucketCount = try guestNumber(memory, hashAddress, count: 4)
        let symbolCount = try guestNumber(memory, try add(hashAddress, 4), count: 4)
        guard bucketCount > 0, bucketCount <= 512, symbolCount > 0,
              symbolCount <= UInt64(maxSymbolCount) else {
            throw GuestDynamicELFError.malformedSymbolTable
        }
        // Ensure the hash table itself is mapped, even though hashing is not yet implemented.
        let hashBytes = try add(8, try add(bucketCount, symbolCount) * 4)
        _ = try memory.read8(try add(hashAddress, hashBytes - 1))
        var symbols: [GuestELFSymbol] = []
        for index in 0..<Int(symbolCount) {
            let addr = try add(symAddress, UInt64(index) * 24)
            let nameOffset = UInt32(try guestNumber(memory, addr, count: 4))
            let info = try memory.read8(try add(addr, 4))
            let sectionIndex = UInt16(try memory.read8(try add(addr, 6))) |
                               (UInt16(try memory.read8(try add(addr, 7))) << 8)
            let value = try guestNumber(memory, try add(addr, 8), count: 8)
            let name = try guestString(memory, strAddress, offset: nameOffset, size: strSize)
            symbols.append(GuestELFSymbol(name: name, defined: sectionIndex != 0,
                                          value: value, binding: info >> 4))
        }
        let needed: [String] = try (tags[1] ?? []).map { value in
            guard value <= UInt64(UInt32.max) else { throw GuestDynamicELFError.malformedDynamicTable }
            return try guestString(memory, strAddress, offset: UInt32(value), size: strSize)
        }
        var exports: [String: UInt64] = [:]
        for sym in symbols where sym.defined && !sym.name.isEmpty && (sym.binding == 1 || sym.binding == 2) {
            exports[sym.name] = try add(loadBias, sym.value)
        }

        // RELA and PLT-RELA point to guest virtual addresses, not raw file offsets.
        var tables: [(UInt64, UInt64)] = []
        if let addr = try tag(7) {
            guard let size = try tag(8), try tag(9) == 24 else {
                throw GuestDynamicELFError.malformedRelocationTable
            }
            tables.append((addr, size))
        } else if try tag(8) != nil { throw GuestDynamicELFError.malformedRelocationTable }
        if let addr = try tag(23) {
            guard let size = try tag(2), try tag(20) == 7 else {
                throw GuestDynamicELFError.malformedRelocationTable
            }
            tables.append((addr, size))
        } else if try tag(2) != nil { throw GuestDynamicELFError.malformedRelocationTable }
        var relocations: [(target: UInt64, info: UInt64, addend: Int64)] = []
        var touched = Set<UInt64>()
        for (addr, size) in tables {
            guard size % 24 == 0, size / 24 <= UInt64(maxRelocations),
                  relocations.count + Int(size / 24) <= maxRelocations else {
                throw GuestDynamicELFError.malformedRelocationTable
            }
            let start = try add(loadBias, addr)
            for n in 0..<Int(size / 24) {
                let item = try add(start, UInt64(n) * 24)
                let target = try add(loadBias, try guestNumber(memory, item, count: 8))
                let info = try guestNumber(memory, try add(item, 8), count: 8)
                let addend = Int64(bitPattern: try guestNumber(memory, try add(item, 16), count: 8))
                guard touched.insert(target).inserted else { throw GuestDynamicELFError.malformedRelocationTable }
                relocations.append((target: target, info: info, addend: addend))
            }
        }
        var boundImports: [String] = []
        for item in relocations {
            let type = UInt32(truncatingIfNeeded: item.info)
            let index = Int(item.info >> 32)
            let value: UInt64
            switch type {
            case 8: // R_X86_64_RELATIVE
                guard index == 0 else { throw GuestDynamicELFError.malformedRelocationTable }
                value = try addSigned(loadBias, item.addend)
            case 6, 7: // R_X86_64_GLOB_DAT / R_X86_64_JUMP_SLOT
                guard index > 0, index < symbols.count else {
                    throw GuestDynamicELFError.malformedSymbolTable
                }
                let symbol = symbols[index]
                guard !symbol.name.isEmpty else { throw GuestDynamicELFError.malformedSymbolTable }
                if symbol.defined {
                    value = try addSigned(try add(loadBias, symbol.value), item.addend)
                } else {
                    var selected: UInt64?
                    for name in needed {
                        if let addr = try? libraries.resolve(library: name, symbol: symbol.name) {
                            selected = addr
                            break
                        }
                    }
                    guard let selected else { throw GuestDynamicELFError.unresolvedSymbol(symbol.name) }
                    value = try addSigned(selected, item.addend)
                    boundImports.append(symbol.name)
                }
            default: throw GuestDynamicELFError.unsupportedRelocation(type)
            }
            try memory.write64(item.target, value: value)
        }
        for segment in segments {
            try memory.protect(at: segment.address, size: segment.size, permissions: segment.permissions)
        }
        return GuestDynamicELFImage(memory: memory, entryPoint: entry,
                                    loadBias: loadBias, neededLibraries: needed,
                                    exports: exports, boundImports: boundImports,
                                    relocationCount: relocations.count)
    }
}
