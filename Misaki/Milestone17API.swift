import Foundation

/// A synthetic module-lifecycle diagnostic; no Sony firmware or PRX is loaded.
struct NativeModuleLifecycleSnapshot {
    let status: Int32
    let abiVersion: UInt32
    let modules: UInt32
    let imports: UInt32
    let instructions: UInt32
    let initializations: UInt32
    let finalizations: UInt32
    let stackRestored: Bool
    let importReadOnly: Bool
    let unloaded: Bool
    let dependencyOrderValid: Bool
    let rax: UInt64

    var passed: Bool {
        status == 0 && abiVersion == 1 && modules == 3 && imports == 1 &&
        instructions == 5 && initializations == 3 && finalizations == 3 &&
        stackRestored && importReadOnly && unloaded && dependencyOrderValid && rax == 42
    }
}

extension NativeCoreAPI {
    static func runModuleLifecycleDiagnostic() -> NativeModuleLifecycleSnapshot {
        var report = MisakiModuleLifecycleReport()
        let status = misaki_core_run_lifecycle_diagnostic(&report)
        return NativeModuleLifecycleSnapshot(
            status: status,
            abiVersion: report.abi_version,
            modules: report.modules_loaded,
            imports: report.linked_imports,
            instructions: report.instructions,
            initializations: report.initializations,
            finalizations: report.finalizations,
            stackRestored: report.stack_restored == 1,
            importReadOnly: report.import_read_only == 1,
            unloaded: report.unloaded == 1,
            dependencyOrderValid: report.dependency_order_valid == 1,
            rax: report.rax
        )
    }
}
