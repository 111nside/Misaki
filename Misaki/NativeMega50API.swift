import Foundation

/// A bounded, SELF-free research diagnostic, not a genuine PS4 firmware boot.
struct NativeMega50Snapshot {
    let status: Int32
    let abiVersion: UInt32
    let stages: [Bool]
    let segments: UInt32
    let relocations: UInt32
    let modules: UInt32
    let imports: UInt32
    let relocationValue: UInt64
    let guestRax: UInt64
    let vmPages: UInt32
    let serviceCalls: UInt32
    let serviceFailures: UInt32
    let pm4Packets: UInt32
    let sprites: UInt32
    let framebufferChecksum: UInt64
    let fence: UInt64
    let firmwareBlockers: UInt32
    let firmwareAttempted: Bool
    let homeMenuBooted: Bool

    var passed: Bool {
        status == 0 && abiVersion == 1 && stages.count == 5 &&
        stages.allSatisfy { $0 } &&
        segments == 2 && relocations == 1 && modules == 2 && imports == 2 &&
        relocationValue == 0x103500 && guestRax == 42 && vmPages == 1 &&
        serviceCalls == 4 && serviceFailures == 0 && pm4Packets == 11 &&
        sprites == 2 && framebufferChecksum != 0 && fence == 1 &&
        firmwareBlockers == 5 && !firmwareAttempted && !homeMenuBooted
    }
}

enum NativeMega50API {
    static func run() -> NativeMega50Snapshot {
        var r = MisakiMega50Report()
        let status = misaki_mega50_diagnostic(&r)
        return NativeMega50Snapshot(
            status: status, abiVersion: r.abi_version,
            stages: [r.pass_46 == 1, r.pass_47 == 1, r.pass_48 == 1,
                     r.pass_49 == 1, r.pass_50 == 1],
            segments: r.segments, relocations: r.relocations,
            modules: r.modules, imports: r.imports,
            relocationValue: r.relocation_value, guestRax: r.guest_rax,
            vmPages: r.vm_pages, serviceCalls: r.service_calls,
            serviceFailures: r.service_failures,
            pm4Packets: r.pm4_packets, sprites: r.sprites,
            framebufferChecksum: r.framebuffer_checksum,
            fence: r.completed_fence, firmwareBlockers: r.firmware_blockers,
            firmwareAttempted: r.firmware_attempted != 0,
            homeMenuBooted: r.home_menu_booted != 0)
    }
}
