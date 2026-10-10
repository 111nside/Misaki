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


/// Milestone 12: ET_DYN dynamic tables -> C++ guest ELF loader/import
/// resolution -> portable x86-64 execution. Test firmware is self-authored.
struct NativeDynamicModuleSnapshot {
    let status: Int32
    let abiVersion: UInt32
    let segments: UInt32
    let imports: UInt32
    let relativeRelocations: UInt32
    let instructions: UInt32
    let halted: Bool
    let stackRestored: Bool
    let importReadOnly: Bool
    let rax: UInt64
    let entry: UInt64
    let linkedAddress: UInt64
    let relativeValue: UInt64

    var passed: Bool {
        status == 0 && abiVersion == 1 && segments == 2 &&
        imports == 1 && relativeRelocations == 1 && instructions == 5 &&
        halted && stackRestored && importReadOnly && rax == 42 &&
        entry == 0x5000 && linkedAddress == 0x9000 && relativeValue == 0x5234
    }
}

extension NativeCoreAPI {
    static func runDynamicModuleDiagnostic() -> NativeDynamicModuleSnapshot {
        var report = MisakiDynamicCPUReport()
        let status = misaki_core_run_dynamic_cpu_diagnostic(&report)
        return NativeDynamicModuleSnapshot(
            status: status,
            abiVersion: report.abi_version,
            segments: report.loaded_segments,
            imports: report.imports,
            relativeRelocations: report.relative_relocations,
            instructions: report.instructions,
            halted: report.halted == 1,
            stackRestored: report.stack_restored == 1,
            importReadOnly: report.import_read_only == 1,
            rax: report.rax,
            entry: report.entry,
            linkedAddress: report.linked_address,
            relativeValue: report.relative_value
        )
    }
}


/// Milestone 13: native toy guest services plus deterministic cooperative
/// scheduling of separately loaded ELF64 test programs. NOT PS4 threads.
struct NativeGuestServicesSnapshot {
    let status: Int32
    let abiVersion: UInt32
    let threads: UInt32
    let instructions: UInt32
    let yields: UInt32
    let serviceCalls: UInt32
    let writeCalls: UInt32
    let outputBytes: UInt32
    let outputMatches: Bool
    let threadsHalted: Bool
    let stacksRestored: Bool
    let firstThreadRAX: UInt64
    let secondThreadRAX: UInt64

    var passed: Bool {
        status == 0 && abiVersion == 1 && threads == 2 &&
        instructions == 30 && yields == 2 && serviceCalls == 8 &&
        writeCalls == 2 && outputBytes == 4 && outputMatches &&
        threadsHalted && stacksRestored &&
        firstThreadRAX == 4097 && secondThreadRAX == 4098
    }
}

extension NativeCoreAPI {
    static func runGuestServicesDiagnostic() -> NativeGuestServicesSnapshot {
        var report = MisakiServiceThreadReport()
        let status = misaki_core_run_services_diagnostic(&report)
        return NativeGuestServicesSnapshot(
            status: status,
            abiVersion: report.abi_version,
            threads: report.thread_count,
            instructions: report.instructions,
            yields: report.yields,
            serviceCalls: report.service_calls,
            writeCalls: report.write_calls,
            outputBytes: report.output_bytes,
            outputMatches: report.output_matches == 1,
            threadsHalted: report.threads_halted == 1,
            stacksRestored: report.stacks_restored == 1,
            firstThreadRAX: report.thread1_rax,
            secondThreadRAX: report.thread2_rax
        )
    }
}

/// Milestone 14: sandboxed, in-memory file services + independent guest PIDs.
/// This is not the PS4 process manager or the iPhone's filesystem.
struct NativeGuestProcessSnapshot {
    let status: Int32
    let abiVersion: UInt32
    let processes: UInt32
    let instructions: UInt32
    let yields: UInt32
    let opens: UInt32
    let reads: UInt32
    let closes: UInt32
    let writes: UInt32
    let outputBytes: UInt32
    let outputMatches: Bool
    let stacksRestored: Bool
    let pid1: UInt64
    let pid2: UInt64

    var passed: Bool {
        status == 0 && abiVersion == 1 && processes == 2 &&
        instructions == 44 && yields == 2 && opens == 2 && reads == 2 &&
        closes == 2 && writes == 2 && outputBytes == 10 &&
        outputMatches && stacksRestored && pid1 == 1001 && pid2 == 1002
    }
}

extension NativeCoreAPI {
    static func runProcessDiagnostic() -> NativeGuestProcessSnapshot {
        var report = MisakiProcessReport()
        let status = misaki_core_run_process_diagnostic(&report)
        return NativeGuestProcessSnapshot(
            status: status,
            abiVersion: report.abi_version,
            processes: report.process_count,
            instructions: report.instructions,
            yields: report.yields,
            opens: report.opens,
            reads: report.reads,
            closes: report.closes,
            writes: report.writes,
            outputBytes: report.output_bytes,
            outputMatches: report.output_matches == 1,
            stacksRestored: report.stacks_restored == 1,
            pid1: report.pid1_rax,
            pid2: report.pid2_rax
        )
    }
}


/// Milestone 15: one integrated ELF64 -> guest library -> process/VFS
/// diagnostic. The callable routines use Misaki's original toy ABI, not
/// PS4 system libraries or host iOS services.
struct NativeSystemLibrarySnapshot {
    let status: Int32
    let abiVersion: UInt32
    let segments: UInt32
    let imports: UInt32
    let relativeRelocations: UInt32
    let instructions: UInt32
    let halted: Bool
    let stackRestored: Bool
    let importReadOnly: Bool
    let libraryReadOnly: Bool
    let processID: UInt32
    let opens: UInt32
    let reads: UInt32
    let closes: UInt32
    let writes: UInt32
    let outputMatches: Bool
    let rax: UInt64
    let entry: UInt64
    let linkedAddress: UInt64

    var passed: Bool {
        status == 0 && abiVersion == 1 && segments == 2 &&
        imports == 1 && relativeRelocations == 1 && instructions == 26 &&
        halted && stackRestored && importReadOnly && libraryReadOnly &&
        processID == 1001 && opens == 1 && reads == 1 &&
        closes == 1 && writes == 1 && outputMatches &&
        rax == 42 && entry == 0x5000 && linkedAddress == 0x9000
    }
}

extension NativeCoreAPI {
    static func runSystemLibraryDiagnostic() -> NativeSystemLibrarySnapshot {
        var report = MisakiSystemLibraryReport()
        let status = misaki_core_run_system_library_diagnostic(&report)
        return NativeSystemLibrarySnapshot(
            status: status,
            abiVersion: report.abi_version,
            segments: report.loaded_segments,
            imports: report.imported_symbols,
            relativeRelocations: report.relative_relocations,
            instructions: report.instructions,
            halted: report.halted == 1,
            stackRestored: report.stack_restored == 1,
            importReadOnly: report.import_read_only == 1,
            libraryReadOnly: report.library_read_only == 1,
            processID: report.process_id,
            opens: report.opens,
            reads: report.reads,
            closes: report.closes,
            writes: report.writes,
            outputMatches: report.output_matches == 1,
            rax: report.rax,
            entry: report.entry,
            linkedAddress: report.linked_address
        )
    }
}


/// The versioned ordinary-ELF catalog and expanded relocation diagnostic.
/// These symbols and relocation fixtures are independently authored test data,
/// not Sony PRX modules or PS4 firmware.
struct NativeLibraryCatalogSnapshot {
    let status: Int32
    let abiVersion: UInt32
    let catalogVersions: UInt32
    let imports: UInt32
    let relativeRelocations: UInt32
    let absoluteRelocations: UInt32
    let pcRelativeRelocations: UInt32
    let instructions: UInt32
    let halted: Bool
    let stackRestored: Bool
    let importReadOnly: Bool
    let rax: UInt64
    let linkedAddress: UInt64
    let absolute64: UInt64
    let absolute32: UInt32
    let pc32: Int32

    var passed: Bool {
        status == 0 && abiVersion == 1 && catalogVersions == 2 &&
        imports == 6 && relativeRelocations == 1 && absoluteRelocations == 3 &&
        pcRelativeRelocations == 2 && instructions == 5 && halted &&
        stackRestored && importReadOnly && rax == 42 &&
        linkedAddress == 0x9000 && absolute64 == 0x8FF8 &&
        absolute32 == 0x9004 && pc32 == 0x3ECC
    }
}

extension NativeCoreAPI {
    static func runLibraryCatalogDiagnostic() -> NativeLibraryCatalogSnapshot {
        var report = MisakiLibraryCatalogReport()
        let status = misaki_core_run_catalog_diagnostic(&report)
        return NativeLibraryCatalogSnapshot(
            status: status,
            abiVersion: report.abi_version,
            catalogVersions: report.catalog_versions,
            imports: report.imported_symbols,
            relativeRelocations: report.relative_relocations,
            absoluteRelocations: report.absolute_relocations,
            pcRelativeRelocations: report.pc_relative_relocations,
            instructions: report.instructions,
            halted: report.halted == 1,
            stackRestored: report.stack_restored == 1,
            importReadOnly: report.import_read_only == 1,
            rax: report.rax,
            linkedAddress: report.linked_address,
            absolute64: report.absolute64,
            absolute32: report.absolute32,
            pc32: report.pc32
        )
    }
}
