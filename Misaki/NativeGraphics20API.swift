import Foundation

/// Milestone 20: a guest-owned queue is read from isolated x86-64 memory;
/// the software graphics backend creates a new RGBA8 framebuffer for Metal.
/// This is NOT the PS4 GPU submission ABI or AMD GCN packet format.
struct NativeGraphics20Frame {
    let status: Int32
    let frameIndex: UInt32
    let width: UInt32
    let height: UInt32
    let commands: UInt32
    let rectangles: UInt32
    let sprites: UInt32
    let guestInstructions: UInt32
    let guestServiceCalls: UInt32
    let guestHalted: Bool
    let stackRestored: Bool
    let queueDrained: Bool
    let spriteX: UInt32
    let spriteY: UInt32
    let submittedFence: UInt64
    let completedFence: UInt64
    let checksum: UInt64
    let pixels: [UInt8]

    var passed: Bool {
        status == 0 && width == 320 && height == 180 &&
        commands == 7 && rectangles == 4 && sprites == 1 &&
        guestInstructions == 9 && guestServiceCalls == 1 &&
        guestHalted && stackRestored && queueDrained &&
        spriteX == 32 + 2 * (frameIndex % 90) && spriteY == 78 &&
        submittedFence == UInt64(frameIndex) + 1 &&
        completedFence == submittedFence && checksum != 0 &&
        pixels.count == 320 * 180 * 4
    }
}

enum NativeGraphics20API {
    static func makeFrame(index: UInt32 = 0) -> NativeGraphics20Frame {
        let capacity = 320 * 180 * 4
        var buffer = [UInt8](repeating: 0, count: capacity)
        var report = MisakiGPU20Report()
        let code: Int32 = buffer.withUnsafeMutableBufferPointer { pointer in
            misaki_gpu20_render(index, pointer.baseAddress, pointer.count, &report)
        }
        return NativeGraphics20Frame(
            status: code, frameIndex: report.frame_index,
            width: report.width, height: report.height,
            commands: report.command_count, rectangles: report.rectangles,
            sprites: report.sprites,
            guestInstructions: report.guest_instructions,
            guestServiceCalls: report.guest_service_calls,
            guestHalted: report.guest_halted == 1,
            stackRestored: report.stack_restored == 1,
            queueDrained: report.queue_drained == 1,
            spriteX: report.sprite_x, spriteY: report.sprite_y,
            submittedFence: report.submitted_fence,
            completedFence: report.completed_fence,
            checksum: report.checksum,
            pixels: code == 0 && Int(report.bytes_written) == capacity ? buffer : []
        )
    }
}
