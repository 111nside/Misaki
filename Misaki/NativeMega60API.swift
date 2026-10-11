import Foundation

/// Milestones 56-60: integration metrics for self-authored fixtures only.
struct NativeMega60Snapshot {
    let status: Int32
    let stages: [Bool]
    let dependencies: UInt32
    let relocations: UInt32
    let serviceCalls: UInt32
    let serviceFailures: UInt32
    let pixels: UInt32
    let gpuCopies: UInt32
    let guestProcesses: UInt32
    let guestExits: UInt32
    let guestInstructions: UInt32
    let contextSwitches: UInt32
    let encryptedSelfSegments: UInt32
    let blockers: UInt32
    let inspectionRefusesExecution: Bool
    let firmwareAttempted: Bool
    let homeMenuBooted: Bool

    var passed: Bool {
        status == 0 && stages == [true, true, true, true, true] &&
        dependencies == 1 && relocations == 1 &&
        serviceCalls == 3 && serviceFailures == 1 &&
        pixels == 256 && gpuCopies == 1 && guestProcesses == 2 &&
        guestExits == 2 && guestInstructions == 16 &&
        encryptedSelfSegments == 1 && inspectionRefusesExecution &&
        blockers == 5 && !firmwareAttempted && !homeMenuBooted
    }
}

/// A read-only inspection result for a user-selected file; unsupported is normal.
struct NativeCompatibility60Snapshot {
    let status: Int32
    let kind: UInt32
    let recognized: Bool
    let encryptedSegments: UInt32
    let partial: Bool
    let executionRefused: Bool
    let neededModules: UInt32
    let findings: UInt32
    let text: String

    var kindLabel: String {
        switch kind {
        case 1: return "SCE ELF64"
        case 2: return "PS4 SELF"
        case 3: return "Ordinary ELF"
        default: return "Unrecognized / unsupported"
        }
    }
}

enum NativeMega60API {
    static func run() -> NativeMega60Snapshot {
        var r = MisakiMega60Report()
        let code = misaki_mega60_diagnostic(&r)
        return NativeMega60Snapshot(
            status: code,
            stages: [r.pass_56 != 0, r.pass_57 != 0, r.pass_58 != 0,
                     r.pass_59 != 0, r.pass_60 != 0],
            dependencies: r.elf_dependencies, relocations: r.rela_records,
            serviceCalls: r.service_calls, serviceFailures: r.service_failures,
            pixels: r.gpu_pixels, gpuCopies: r.gpu_copies,
            guestProcesses: r.guest_processes, guestExits: r.guest_exits,
            guestInstructions: r.guest_instructions,
            contextSwitches: r.context_switches,
            encryptedSelfSegments: r.encrypted_self_segments,
            blockers: r.firmware_blockers,
            inspectionRefusesExecution: r.analysis_refused_execution != 0,
            firmwareAttempted: r.firmware_attempted != 0,
            homeMenuBooted: r.home_menu_booted != 0)
    }

    /// Analyzes the first up-to-8MiB of an executable without executing it.
    /// `totalSize` must reflect the actual file length, not just the prefix.
    static func analyze(prefix: Data, totalSize: UInt64) -> NativeCompatibility60Snapshot {
        var report = MisakiCompatibility60Report()
        var buffer = [CChar](repeating: 0, count: 4096)
        let status: Int32 = buffer.withUnsafeMutableBufferPointer { destination in
            prefix.withUnsafeBytes { source in
                misaki_compat60_inspect(source.bindMemory(to: UInt8.self).baseAddress,
                                        source.count, totalSize, &report,
                                        destination.baseAddress, destination.count)
            }
        }
        let text: String
        if status == 0 {
            let utf8 = buffer.prefix(while: { $0 != 0 }).map { UInt8(bitPattern: $0) }
            text = String(decoding: utf8, as: UTF8.self)
        } else {
            text = "File inspection failed (status \(status)). No executable was loaded."
        }
        return NativeCompatibility60Snapshot(
            status: status, kind: report.kind, recognized: report.recognized != 0,
            encryptedSegments: report.encrypted_segments, partial: report.partial != 0,
            executionRefused: report.execution_refused != 0,
            neededModules: report.needed_modules, findings: report.findings,
            text: text)
    }
}
