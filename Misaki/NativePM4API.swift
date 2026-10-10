import Foundation

/// Diagnostic for AMD GCN-era PM4 *packet decoding*, not PS4 GPU execution.
struct NativePM4Register: Identifiable {
    let id: Int
    let address: UInt32
    let value: UInt32
    let packetIndex: UInt32
    let computeShader: Bool
}

struct NativePM4Snapshot {
    let status: Int32
    let abiVersion: UInt32
    let packets: UInt32
    let type0Packets: UInt32
    let type2Packets: UInt32
    let type3Packets: UInt32
    let nops: UInt32
    let registerWrites: UInt32
    let contextWrites: UInt32
    let shaderWrites: UInt32
    let otherWrites: UInt32
    let draws: UInt32
    let events: UInt32
    let lastVertexCount: UInt32
    let guestMemoryVerified: Bool
    let checksum: UInt64
    let registers: [NativePM4Register]

    var passed: Bool {
        status == 0 && abiVersion == 1 && packets == 9 &&
        type0Packets == 0 && type2Packets == 1 && type3Packets == 8 &&
        nops == 1 && registerWrites == 4 && contextWrites == 2 &&
        shaderWrites == 1 && otherWrites == 1 && draws == 1 &&
        events == 1 && lastVertexCount == 3 && guestMemoryVerified &&
        checksum != 0 && registers.count == 4 &&
        registers.first?.address == 0xA0B4 &&
        registers.last?.address == 0xC002
    }
}

enum NativePM4API {
    static func runDiagnostic() -> NativePM4Snapshot {
        let capacity = 256
        var writes = [MisakiPM4RegisterWrite](
            repeating: MisakiPM4RegisterWrite(), count: capacity)
        var report = MisakiPM4TraceReport()
        let status: Int32 = writes.withUnsafeMutableBufferPointer { buffer in
            misaki_pm4_demo_trace(buffer.baseAddress, buffer.count, &report)
        }
        let count = status == 0 && report.register_writes <= UInt32(capacity)
            ? Int(report.register_writes) : 0
        let registers = writes.prefix(count).enumerated().map { index, write in
            NativePM4Register(id: index, address: write.address,
                              value: write.value, packetIndex: write.packet_index,
                              computeShader: write.shader_type == 1)
        }
        return NativePM4Snapshot(
            status: status, abiVersion: report.abi_version,
            packets: report.packet_count, type0Packets: report.type0_packets,
            type2Packets: report.type2_packets, type3Packets: report.type3_packets,
            nops: report.nop_packets, registerWrites: report.register_writes,
            contextWrites: report.context_writes, shaderWrites: report.shader_writes,
            otherWrites: report.other_writes, draws: report.draw_auto_packets,
            events: report.event_writes, lastVertexCount: report.last_vertex_count,
            guestMemoryVerified: report.guest_memory_verified == 1,
            checksum: report.checksum, registers: registers)
    }
}
