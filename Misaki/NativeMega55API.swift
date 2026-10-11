import Foundation

/// An intentionally limited compatibility diagnostic, not PS4 firmware booting.
struct NativeMega55Snapshot {
    let status: Int32
    let abiVersion: UInt32
    let stages: [Bool]
    let elfSegments: UInt32
    let elfRelocations: UInt32
    let selfEncryptedSegments: UInt32
    let dynamicTags: UInt32
    let syntheticNids: UInt32
    let processes: UInt32
    let mappedPages: UInt32
    let pm4Packets: UInt32
    let gcnLanes: UInt32
    let gcnInstructions: UInt32
    let outputRecord7: UInt32
    let guestRax: UInt64
    let shaderHash: UInt64
    let encryptedSelfRefused: Bool
    let firmwareBlockers: UInt32
    let firmwareAttempted: Bool
    let homeMenuBooted: Bool

    var passed: Bool {
        status == 0 && abiVersion == 1 && stages == [true, true, true, true, true] &&
        elfSegments == 2 && elfRelocations == 1 && selfEncryptedSegments == 1 &&
        dynamicTags == 3 && syntheticNids == 2 && processes == 1 &&
        mappedPages == 2 && pm4Packets == 11 && gcnLanes == 4 &&
        gcnInstructions == 44 && outputRecord7 == 31 &&
        guestRax == 42 && shaderHash != 0 && encryptedSelfRefused &&
        firmwareBlockers == 5 && !firmwareAttempted && !homeMenuBooted
    }
}

enum NativeMega55API {
    static func run() -> NativeMega55Snapshot {
        var r = MisakiMega55Report()
        let code = misaki_mega55_diagnostic(&r)
        return NativeMega55Snapshot(
            status: code, abiVersion: r.abi_version,
            stages: [r.pass_51 != 0, r.pass_52 != 0, r.pass_53 != 0,
                     r.pass_54 != 0, r.pass_55 != 0],
            elfSegments: r.elf_segments, elfRelocations: r.elf_relocations,
            selfEncryptedSegments: r.self_encrypted_segments,
            dynamicTags: r.sce_dynamic_tags, syntheticNids: r.synthetic_nids,
            processes: r.processes, mappedPages: r.mapped_pages,
            pm4Packets: r.pm4_packets, gcnLanes: r.gcn_lanes,
            gcnInstructions: r.gcn_instructions, outputRecord7: r.output_index_7,
            guestRax: r.guest_rax, shaderHash: r.shader_hash,
            encryptedSelfRefused: r.encrypted_self_refused != 0,
            firmwareBlockers: r.firmware_blockers,
            firmwareAttempted: r.firmware_attempted != 0,
            homeMenuBooted: r.home_menu_booted != 0)
    }
}
