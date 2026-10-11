import Foundation

/// Milestones 41–45: synthetic PS4-shaped loader, imports, service and GPU
/// diagnostics. None of these checks execute genuine PS4 firmware.
struct NativePS4PhaseTwoSnapshot {
    let status: Int32
    let abiVersion: UInt32
    let stages: [Bool]
    let segments: UInt32
    let relocations: UInt32
    let imports: UInt32
    let serviceCalls: UInt32
    let guestInstructions: UInt32
    let outputBytes: UInt32
    let guestRAX: UInt64
    let relocationValue: UInt64
    let pm4Packets: UInt32
    let gpuPackets: UInt32
    let sprites: UInt32
    let fence: UInt64
    let framebufferChecksum: UInt64
    let realBootBlockers: UInt32
    let firmwareAttempted: Bool
    let homeMenuBooted: Bool

    var passed: Bool {
        status == 0 && abiVersion == 1 && stages.count == 5 &&
        stages.allSatisfy { $0 } && segments == 1 && relocations == 1 &&
        imports == 2 && serviceCalls == 3 && guestInstructions > 0 &&
        outputBytes == 5 && guestRAX == 42 && relocationValue == 0x102000 &&
        pm4Packets == 11 && gpuPackets == 10 && sprites == 2 && fence == 1 &&
        framebufferChecksum != 0 && realBootBlockers == 5 &&
        !firmwareAttempted && !homeMenuBooted
    }
}

enum NativePS4PhaseTwoAPI {
    static func run() -> NativePS4PhaseTwoSnapshot {
        var report = MisakiPS4PhaseTwoReport()
        let status = misaki_ps4_phase_two_diagnostic(&report)
        return NativePS4PhaseTwoSnapshot(
            status: status, abiVersion: report.abi_version,
            stages: [report.passed_41 == 1, report.passed_42 == 1,
                     report.passed_43 == 1, report.passed_44 == 1,
                     report.passed_45 == 1],
            segments: report.mapped_segments,
            relocations: report.relative_relocations,
            imports: report.imports,
            serviceCalls: report.service_calls,
            guestInstructions: report.instructions,
            outputBytes: report.output_bytes,
            guestRAX: report.guest_rax,
            relocationValue: report.relocation_value,
            pm4Packets: report.pm4_packets,
            gpuPackets: report.gpu_packets,
            sprites: report.sprites,
            fence: report.completed_fence,
            framebufferChecksum: report.framebuffer_checksum,
            realBootBlockers: report.real_boot_blockers,
            firmwareAttempted: report.firmware_attempted != 0,
            homeMenuBooted: report.home_menu_booted != 0)
    }
}
