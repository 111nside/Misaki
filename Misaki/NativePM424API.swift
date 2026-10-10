import Foundation

/// Read-only interpretation of a subset of authentic AMD PM4 register fields.
/// Consistent metadata is NOT proof that a PS4 render target or shader will run.
struct NativePM424Draw: Identifiable {
    let id: Int
    let packetAddress: UInt64
    let colorAddress: UInt64
    let pixelShaderAddress: UInt64
    let packetIndex: UInt32
    let depth: UInt32
    let vertices: UInt32
    let scissorX: UInt32
    let scissorY: UInt32
    let scissorWidth: UInt32
    let scissorHeight: UInt32
    let pitchTileMax: UInt32
    let colorFormatField: UInt32
    let targetMask: UInt32
    let observedMask: UInt32
    let stateStatus: UInt32

    var metadataComplete: Bool { stateStatus == 0 }
}

struct NativePM424Snapshot {
    let status: Int32
    let abiVersion: UInt32
    let packets: UInt32
    let indirectBuffers: UInt32
    let registerWrites: UInt32
    let drawCount: UInt32
    let readyDraws: UInt32
    let rejectedDraws: UInt32
    let checksum: UInt64
    let draws: [NativePM424Draw]

    var passed: Bool {
        guard status == 0, abiVersion == 1, packets == 11,
              indirectBuffers == 2, registerWrites == 8,
              drawCount == 1, readyDraws == 1, rejectedDraws == 0,
              checksum != 0, draws.count == 1 else { return false }
        let d = draws[0]
        return d.metadataComplete && d.packetIndex == 9 && d.depth == 1 &&
               d.vertices == 3 && d.packetAddress == 0x5002C &&
               d.colorAddress == 0x20000000 &&
               d.pixelShaderAddress == 0x100000 &&
               d.scissorX == 10 && d.scissorY == 10 &&
               d.scissorWidth == 190 && d.scissorHeight == 110 &&
               d.pitchTileMax == 127 && d.colorFormatField == 27 &&
               d.targetMask == 15 && d.observedMask == 255
    }
}

enum NativePM424API {
    static func runDiagnostic() -> NativePM424Snapshot {
        let capacity = 64
        var buffer = [MisakiPM424Draw](repeating: MisakiPM424Draw(), count: capacity)
        var report = MisakiPM424Report()
        let status: Int32 = buffer.withUnsafeMutableBufferPointer { pointer in
            misaki_pm424_diagnostic(pointer.baseAddress, pointer.count, &report)
        }
        let count = status == 0 && report.draws <= UInt32(capacity)
            ? Int(report.draws) : 0
        let draws = buffer.prefix(count).enumerated().map { i, d in
            NativePM424Draw(
                id: i, packetAddress: d.packet_address,
                colorAddress: d.color_address,
                pixelShaderAddress: d.pixel_shader_address,
                packetIndex: d.packet_index, depth: d.depth,
                vertices: d.vertex_count,
                scissorX: d.scissor_x, scissorY: d.scissor_y,
                scissorWidth: d.scissor_width,
                scissorHeight: d.scissor_height,
                pitchTileMax: d.pitch_tile_max,
                colorFormatField: d.color_format_field,
                targetMask: d.target_mask,
                observedMask: d.observed_mask,
                stateStatus: d.state_status)
        }
        return NativePM424Snapshot(
            status: status, abiVersion: report.abi_version,
            packets: report.packets, indirectBuffers: report.indirect_buffers,
            registerWrites: report.register_writes, drawCount: report.draws,
            readyDraws: report.ready_draws, rejectedDraws: report.rejected_draws,
            checksum: report.checksum, draws: draws)
    }
}
