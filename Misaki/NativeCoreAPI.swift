import Foundation

/// Keeps the C bridge private to the app module. Swift unit tests use this
/// Swift-facing API rather than depending on imported C declarations directly.
struct NativeCoreSnapshot {
    let status: Int32
    let version: String
    let abiVersion: UInt32
    let elfType: UInt32
    let loadSegments: UInt32
    let registeredModules: UInt32
    let resolvedImports: UInt32
    let importReadOnly: Bool
    let linkedGuestAddress: UInt64

    var passed: Bool {
        status == 0 && abiVersion == 1 && elfType == 3 &&
        loadSegments == 1 && registeredModules == 1 &&
        resolvedImports == 1 && importReadOnly && linkedGuestAddress == 0x6200
    }
}

enum NativeCoreAPI {
    static var version: String { String(cString: misaki_core_version()) }

    static func runDiagnostic() -> NativeCoreSnapshot {
        var report = MisakiCoreReport()
        let status = misaki_core_run_diagnostic(&report)
        return NativeCoreSnapshot(
            status: status,
            version: version,
            abiVersion: report.abi_version,
            elfType: report.elf_type,
            loadSegments: report.load_segments,
            registeredModules: report.registered_modules,
            resolvedImports: report.resolved_imports,
            importReadOnly: report.import_is_read_only == 1,
            linkedGuestAddress: report.linked_guest_address
        )
    }

    static func inspectELF(_ bytes: [UInt8]) -> (code: Int32, type: UInt32, segments: UInt32) {
        var type: UInt32 = 0
        var segments: UInt32 = 0
        let code = bytes.withUnsafeBufferPointer { ptr in
            misaki_core_inspect_elf(ptr.baseAddress, ptr.count, &type, &segments)
        }
        return (code, type, segments)
    }
}
