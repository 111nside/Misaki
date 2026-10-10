import Foundation

/// Host-neutral drawing commands produced by Misaki's original C++ test GPU.
/// This is a prototype 2D drawing stream, NOT a PS4 graphics API.
struct NativeGPUCommand {
    let kind: UInt32
    let x: UInt32
    let y: UInt32
    let width: UInt32
    let height: UInt32
    let rgba: UInt32
}

struct NativeGPUFrame {
    let status: Int32
    let width: UInt32
    let height: UInt32
    let commandCount: UInt32
    let rectangles: UInt32
    let clears: UInt32
    let presents: UInt32
    let checksum: UInt64
    let commands: [NativeGPUCommand]

    var passed: Bool {
        status == 0 && width == 320 && height == 180 &&
        commandCount == 7 && rectangles == 5 && clears == 1 && presents == 1 &&
        commands.count == 7 && checksum != 0 &&
        commands.first?.kind == 1 && commands.last?.kind == 3
    }
}

enum NativeGPUAPI {
    static func makeDemoFrame() -> NativeGPUFrame {
        let capacity = 64
        var buffer = [MisakiGPUCommand](repeating: MisakiGPUCommand(), count: capacity)
        var info = MisakiGPUFrameInfo()
        let status: Int32 = buffer.withUnsafeMutableBufferPointer { pointer in
            misaki_gpu_demo_frame(pointer.baseAddress, pointer.count, &info)
        }
        let count = status == 0 && Int(info.command_count) <= capacity
            ? Int(info.command_count) : 0
        let commands = buffer.prefix(count).map {
            NativeGPUCommand(kind: $0.kind, x: $0.x, y: $0.y,
                             width: $0.width, height: $0.height, rgba: $0.rgba)
        }
        return NativeGPUFrame(status: status, width: info.width, height: info.height,
                              commandCount: info.command_count, rectangles: info.rectangle_count,
                              clears: info.clear_count, presents: info.present_count,
                              checksum: info.checksum, commands: commands)
    }
}
