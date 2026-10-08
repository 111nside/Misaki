import Foundation

/// Research-only guest module and symbol registry. This is not a PS4 PRX/SCE NID resolver.
struct GuestLibrary {
    let name: String
    let exports: [String: UInt64]
}

struct GuestImport {
    let library: String
    let symbol: String
    let slotAddress: UInt64
}

enum GuestLinkError: Error, Equatable, LocalizedError {
    case duplicateLibrary(String)
    case invalidLibrary
    case unresolvedLibrary(String)
    case unresolvedSymbol(String, String)

    var errorDescription: String? {
        switch self {
        case .duplicateLibrary(let name): return "Guest library already registered: \(name)"
        case .invalidLibrary: return "The guest library or exported name is invalid."
        case .unresolvedLibrary(let name): return "Missing guest library: \(name)"
        case .unresolvedSymbol(let library, let symbol):
            return "Missing symbol \(symbol) in guest library \(library)"
        }
    }
}

struct GuestLinkReport {
    let resolvedSymbols: [String]
    var count: Int { resolvedSymbols.count }
}

struct GuestDynamicLinker {
    private(set) var libraries: [String: GuestLibrary] = [:]

    mutating func register(_ library: GuestLibrary) throws {
        guard !library.name.isEmpty,
              library.exports.keys.allSatisfy({ !$0.isEmpty }),
              library.exports.values.allSatisfy({ $0 != 0 }) else {
            throw GuestLinkError.invalidLibrary
        }
        guard libraries[library.name] == nil else {
            throw GuestLinkError.duplicateLibrary(library.name)
        }
        libraries[library.name] = library
    }

    func resolve(library: String, symbol: String) throws -> UInt64 {
        guard let exports = libraries[library]?.exports else {
            throw GuestLinkError.unresolvedLibrary(library)
        }
        guard let address = exports[symbol] else {
            throw GuestLinkError.unresolvedSymbol(library, symbol)
        }
        return address
    }

    /// Simulates filling an ELF GOT/import table by resolving explicit import descriptors.
    /// Import descriptors aren't parsed from SELF, PRX, or .dynamic yet.
    /// Link writes are transactional: memory changes only if every write succeeds.
    func link(_ imports: [GuestImport], into memory: inout VirtualMemory) throws -> GuestLinkReport {
        let bindings = try imports.map { item in
            (item, try resolve(library: item.library, symbol: item.symbol))
        }
        var copy = memory
        for (item, address) in bindings {
            try copy.write64(item.slotAddress, value: address)
        }
        memory = copy
        return GuestLinkReport(resolvedSymbols: bindings.map { "\($0.0.library)::\($0.0.symbol)" })
    }
}
