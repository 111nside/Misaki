import Foundation

/// One cooperative diagnostic session containing two isolated guest x86-64
/// processes. It is a research runtime, not a PlayStation kernel or firmware.
struct NativeIntegratedFrame {
    let status: Int32
    let stages: [Bool]
    let processCount: UInt32
    let instructions: UInt32
    let contextSwitches: UInt32
    let fileOpens: UInt32
    let fileReads: UInt32
    let fileCloses: UInt32
    let gpuCalls: UInt32
    let pm4Packets: UInt32
    let shaderOperations: UInt32
    let loadedModules: UInt32
    let resolvedImports: UInt32
    let sprites: UInt32
    let submittedFence: UInt64
    let completedFence: UInt64
    let elfRAX: UInt64
    let guestPID: UInt64
    let outputRecord7: UInt32
    let width: UInt32
    let height: UInt32
    let checksum: UInt64
    let pixels: [UInt8]
    let effectMode: UInt32 = 0

    var passed: Bool {
        status == 0 && stages.count == 5 && stages.allSatisfy { $0 } &&
        processCount == 2 && instructions > 0 && contextSwitches > 2 &&
        fileOpens == 1 && fileReads == 1 && fileCloses == 1 &&
        gpuCalls == 1 && pm4Packets == 11 && shaderOperations == 11 &&
        loadedModules == 2 && resolvedImports == 1 && sprites == 2 &&
        submittedFence == 1 && completedFence == 1 &&
        elfRAX == 42 && guestPID == 1001 && outputRecord7 == 31 &&
        width == 320 && height == 180 && checksum != 0 &&
        pixels.count == 320 * 180 * 4
    }
    var surfaceReady: Bool { passed }
}

enum NativeIntegratedRuntimeAPI {
    static func run() -> NativeIntegratedFrame {
        let capacity = 320 * 180 * 4
        var pixels = [UInt8](repeating: 0, count: capacity)
        var report = MisakiIntegratedRuntimeReport()
        let status: Int32 = pixels.withUnsafeMutableBufferPointer { buffer in
            misaki_integrated_runtime_demo(buffer.baseAddress, buffer.count, &report)
        }
        return NativeIntegratedFrame(
            status: status,
            stages: [report.passed_31 == 1, report.passed_32 == 1,
                     report.passed_33 == 1, report.passed_34 == 1,
                     report.passed_35 == 1],
            processCount: report.process_count,
            instructions: report.instructions,
            contextSwitches: report.context_switches,
            fileOpens: report.vfs_opens,
            fileReads: report.vfs_reads,
            fileCloses: report.vfs_closes,
            gpuCalls: report.gpu_calls,
            pm4Packets: report.pm4_packets,
            shaderOperations: report.shader_operations,
            loadedModules: report.loaded_modules,
            resolvedImports: report.resolved_imports,
            sprites: report.framebuffer_sprites,
            submittedFence: report.submitted_fence,
            completedFence: report.completed_fence,
            elfRAX: report.elf_rax,
            guestPID: report.guest_pid,
            outputRecord7: report.output_record_7,
            width: report.framebuffer_width,
            height: report.framebuffer_height,
            checksum: report.framebuffer_checksum,
            pixels: status == 0 && Int(report.bytes_written) == capacity ? pixels : [])
    }
}
