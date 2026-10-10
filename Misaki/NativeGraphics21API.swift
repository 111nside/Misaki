import Foundation

/// Native Milestone 21: self-authored guest x86-64 code programs two sprites,
/// a render-state register and a command queue. No PS4/GCN compatibility.
struct NativeGraphics21Frame {
    let status: Int32
    let frameIndex: UInt32
    let width: UInt32
    let height: UInt32
    let commands: UInt32
    let rectangles: UInt32
    let sprites: UInt32
    let distinctTextures: UInt32
    let stateChanges: UInt32
    let scissorChanges: UInt32
    let effectMode: UInt32
    let guestInstructions: UInt32
    let guestServiceCalls: UInt32
    let guestHalted: Bool
    let stackRestored: Bool
    let queueDrained: Bool
    let firstSpriteX: UInt32
    let secondSpriteX: UInt32
    let submittedFence: UInt64
    let completedFence: UInt64
    let checksum: UInt64
    let pixels: [UInt8]

    var passed: Bool {
        status == 0 && width == 320 && height == 180 &&
        commands == 10 && rectangles == 2 && sprites == 2 &&
        distinctTextures == 2 && stateChanges == 2 && scissorChanges == 2 &&
        effectMode <= 3 && guestInstructions == 15 && guestServiceCalls == 1 &&
        guestHalted && stackRestored && queueDrained &&
        firstSpriteX == 32 + 2 * (frameIndex % 90) &&
        secondSpriteX == 208 - (frameIndex % 90) &&
        submittedFence == UInt64(frameIndex) + 1 &&
        completedFence == submittedFence && checksum != 0 &&
        pixels.count == 320 * 180 * 4
    }
}

enum NativeGraphics21API {
    static func makeFrame(index: UInt32 = 0, effect: UInt32 = 0) -> NativeGraphics21Frame {
        let capacity = 320 * 180 * 4
        var buffer = [UInt8](repeating: 0, count: capacity)
        var report = MisakiGPU21Report()
        let status: Int32 = buffer.withUnsafeMutableBufferPointer { pointer in
            misaki_gpu21_render(index, effect, pointer.baseAddress, pointer.count, &report)
        }
        return NativeGraphics21Frame(
            status: status, frameIndex: report.frame_index,
            width: report.width, height: report.height,
            commands: report.command_count, rectangles: report.rectangles,
            sprites: report.sprites, distinctTextures: report.distinct_textures,
            stateChanges: report.state_changes, scissorChanges: report.scissor_changes,
            effectMode: report.effect_mode,
            guestInstructions: report.guest_instructions,
            guestServiceCalls: report.guest_service_calls,
            guestHalted: report.guest_halted == 1,
            stackRestored: report.stack_restored == 1,
            queueDrained: report.queue_drained == 1,
            firstSpriteX: report.first_sprite_x, secondSpriteX: report.second_sprite_x,
            submittedFence: report.submitted_fence, completedFence: report.completed_fence,
            checksum: report.checksum,
            pixels: status == 0 && Int(report.bytes_written) == capacity ? buffer : []
        )
    }
}
