import Foundation

/// A verified *negative* firmware boot-readiness report; no PS4 firmware is run.
struct NativePS4BootSnapshot {
    let status: Int32
    let version: UInt32
    let stages: [Bool]
    let selfSegments: UInt32
    let encryptedSegments: UInt32
    let sceELFType: UInt32
    let syntheticImports: UInt32
    let gpuLanes: UInt32
    let gcnInstructions: UInt32
    let processes: UInt32
    let x64Instructions: UInt32
    let pm4Packets: UInt32
    let sprites: UInt32
    let readinessBlockers: UInt32
    let firmwareAttempted: Bool
    let homeMenuBooted: Bool
    let fence: UInt64

    var passed: Bool {
        status == 0 && version == 1 && stages.count == 5 &&
        stages.allSatisfy { $0 } && selfSegments == 1 &&
        encryptedSegments == 1 && sceELFType == 0xFE10 &&
        syntheticImports == 2 && gpuLanes == 4 &&
        gcnInstructions == 44 && processes == 2 &&
        x64Instructions > 0 && pm4Packets == 11 && sprites == 2 &&
        readinessBlockers == 5 && !firmwareAttempted &&
        !homeMenuBooted && fence == 1
    }
}

enum NativePS4BootAPI {
    static func run() -> NativePS4BootSnapshot {
        var report = MisakiPS4BootReport()
        let status = misaki_ps4_boot_readiness_diagnostic(&report)
        return NativePS4BootSnapshot(
            status: status, version: report.abi_version,
            stages: [report.passed_36 == 1, report.passed_37 == 1,
                     report.passed_38 == 1, report.passed_39 == 1,
                     report.passed_40 == 1],
            selfSegments: report.synthetic_self_segments,
            encryptedSegments: report.encrypted_segments,
            sceELFType: report.sce_elf_type,
            syntheticImports: report.resolved_synthetic_imports,
            gpuLanes: report.cooperative_gcn_lanes,
            gcnInstructions: report.gcn_instructions,
            processes: report.processes,
            x64Instructions: report.x64_instructions,
            pm4Packets: report.pm4_packets,
            sprites: report.framebuffer_sprites,
            readinessBlockers: report.readiness_blockers,
            firmwareAttempted: report.firmware_execution_attempted != 0,
            homeMenuBooted: report.home_menu_booted != 0,
            fence: report.completed_gpu_fence)
    }
}
