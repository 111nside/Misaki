import Foundation

/// Read-only AMD PM4 decoding trace. This is not command execution or a PS4 GPU.
struct NativePM423Packet: Identifiable {
    let id: Int
    let guestAddress: UInt64
    let type: UInt32
    let opcode: UInt32
    let depth: UInt32
    let bodyWords: UInt32

    var label: String {
        if type == 2 { return "PACKET2 padding" }
        if type == 0 { return "PACKET0 register write" }
        switch opcode {
        case 0x3F: return "INDIRECT_BUFFER"
        case 0x33: return "INDIRECT_BUFFER_CONST"
        case 0x69: return "SET_CONTEXT_REG"
        case 0x76: return "SET_SH_REG"
        case 0x2D: return "DRAW_INDEX_AUTO"
        case 0x46: return "EVENT_WRITE"
        case 0x10: return "NOP"
        default: return "PACKET3 0x\(String(opcode, radix: 16).uppercased())"
        }
    }
}

struct NativePM423Snapshot {
    let status: Int32
    let abiVersion: UInt32
    let packetCount: UInt32
    let indirectBuffers: UInt32
    let indirectConstBuffers: UInt32
    let maximumDepth: UInt32
    let guestWords: UInt32
    let registerWrites: UInt32
    let drawPackets: UInt32
    let eventPackets: UInt32
    let observedRegisterMask: UInt32
    let targetMetadataObserved: Bool
    let colorBase: UInt32
    let colorPitch: UInt32
    let colorInfo: UInt32
    let targetMask: UInt32
    let scissorTopLeft: UInt32
    let scissorBottomRight: UInt32
    let pixelShaderLow: UInt32
    let pixelShaderHigh: UInt32
    let checksum: UInt64
    let packets: [NativePM423Packet]

    var passed: Bool {
        status == 0 && abiVersion == 1 && packetCount == 11 &&
        indirectBuffers == 2 && indirectConstBuffers == 1 &&
        maximumDepth == 2 && guestWords == 34 && registerWrites == 8 &&
        drawPackets == 1 && eventPackets == 1 &&
        observedRegisterMask == 255 && targetMetadataObserved &&
        colorBase == 0x200000 && colorPitch == 127 && colorInfo == 0x1B &&
        targetMask == 15 && scissorTopLeft == 0x000A000A &&
        scissorBottomRight == 0x007800C8 && pixelShaderLow == 0x1000 &&
        pixelShaderHigh == 0 && checksum != 0 && packets.count == 11 &&
        packets.filter { $0.depth == 2 }.count == 3
    }
}

enum NativePM423API {
    static func runDiagnostic() -> NativePM423Snapshot {
        let capacity = 32
        var buffer = [MisakiPM423Packet](repeating: MisakiPM423Packet(), count: capacity)
        var report = MisakiPM423Report()
        let status: Int32 = buffer.withUnsafeMutableBufferPointer { pointer in
            misaki_pm423_diagnostic(pointer.baseAddress, pointer.count, &report)
        }
        let count = status == 0 && report.packet_count <= UInt32(capacity)
            ? Int(report.packet_count) : 0
        let packets = buffer.prefix(count).enumerated().map { index, packet in
            NativePM423Packet(id: index, guestAddress: packet.guest_address,
                              type: packet.type, opcode: packet.opcode,
                              depth: packet.depth, bodyWords: packet.body_words)
        }
        return NativePM423Snapshot(
            status: status, abiVersion: report.abi_version,
            packetCount: report.packet_count, indirectBuffers: report.indirect_buffers,
            indirectConstBuffers: report.indirect_const_buffers,
            maximumDepth: report.maximum_depth, guestWords: report.guest_words,
            registerWrites: report.register_writes,
            drawPackets: report.draw_auto_packets,
            eventPackets: report.event_writes,
            observedRegisterMask: report.observed_register_mask,
            targetMetadataObserved: report.render_target_metadata_observed == 1,
            colorBase: report.color0_base, colorPitch: report.color0_pitch,
            colorInfo: report.color0_info, targetMask: report.color_target_mask,
            scissorTopLeft: report.scissor_tl, scissorBottomRight: report.scissor_br,
            pixelShaderLow: report.pixel_shader_low,
            pixelShaderHigh: report.pixel_shader_high, checksum: report.checksum,
            packets: packets)
    }
}
