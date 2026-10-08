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

/// The C++ guest CPU's synthetic ELF and linked-library execution result.
/// Does not report PS4 firmware, kernel, or game compatibility.
struct NativeCPUExecutionSnapshot {
    let status: Int32
    let abiVersion: UInt32
    let loadedSegments: UInt32
    let imports: UInt32
    let instructions: UInt32
    let halted: Bool
    let stackRestored: Bool
    let importReadOnly: Bool
    let rax: UInt64
    let linkedAddress: UInt64

    var passed: Bool {
        status == 0 && abiVersion == 1 && loadedSegments == 2 && imports == 1 &&
        instructions == 5 && halted && stackRestored && importReadOnly &&
        rax == 42 && linkedAddress == 0x3000
    }
}

extension NativeCoreAPI {
    static func runCPUExecutionDiagnostic() -> NativeCPUExecutionSnapshot {
        var report = MisakiNativeCPUReport()
        let status = misaki_core_run_cpu_diagnostic(&report)
        return NativeCPUExecutionSnapshot(
            status: status,
            abiVersion: report.abi_version,
            loadedSegments: report.loaded_segments,
            imports: report.resolved_imports,
            instructions: report.instructions,
            halted: report.halted == 1,
            stackRestored: report.stack_restored == 1,
            importReadOnly: report.import_read_only == 1,
            rax: report.rax,
            linkedAddress: report.linked_address
        )
    }
}

/// Milestone 11's expandable native C++ CPU backend.
struct NativeX64BackendSnapshot {
    let status: Int32
    let abiVersion: UInt32
    let backendId: UInt32
    let instructions: UInt32
    let halted: Bool
    let stackRestored: Bool
    let zeroFlag: Bool
    let imports: UInt32
    let importReadOnly: Bool
    let rax: UInt64
    let linkedAddress: UInt64

    var passed: Bool {
        status == 0 && abiVersion == 1 && backendId == 1 &&
        instructions == 13 && halted && stackRestored &&
        !zeroFlag && imports == 1 && importReadOnly &&
        rax == 44 && linkedAddress == 0x3000
    }
}

extension NativeCoreAPI {
    static func runX64BackendDiagnostic() -> NativeX64BackendSnapshot {
        var report = MisakiX64BackendReport()
        let status = misaki_core_run_backend_diagnostic(&report)
        return NativeX64BackendSnapshot(
            status: status,
            abiVersion: report.abi_version,
            backendId: report.backend_id,
            instructions: report.instructions,
            halted: report.halted == 1,
            stackRestored: report.stack_restored == 1,
            zeroFlag: report.zero_flag == 1,
            imports: report.resolved_imports,
            importReadOnly: report.import_read_only == 1,
            rax: report.rax,
            linkedAddress: report.linked_address
        )
    }
}
