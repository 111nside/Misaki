import Foundation

/// Diagnostic for Misaki's *bounded* scalar/vector GCN subset, not PS4 wavefront
/// execution, shader translation, or Metal GPU compatibility.
struct NativeShaderIROperation: Identifiable {
    let id: Int
    let guestAddress: UInt64
    let wordOffset: UInt32
    let opcode: UInt32
    let destination: UInt32
    let source0Kind: UInt32
    let source0Value: UInt32
    let source1Kind: UInt32
    let source1Value: UInt32

    var mnemonic: String {
        switch opcode {
        case 1: return "s_movk_i32"
        case 2: return "s_mov_b32"
        case 3: return "s_not_b32"
        case 4: return "s_add_u32"
        case 5: return "s_sub_u32"
        case 6: return "s_and_b32"
        case 7: return "v_mov_b32"
        case 8: return "v_add_f32"
        case 9: return "v_mul_f32"
        case 10: return "s_nop"
        case 11: return "s_endpgm"
        default: return "unknown_\(opcode)"
        }
    }

    private func source(_ kind: UInt32, _ value: UInt32) -> String {
        switch kind {
        case 1: return "s\(value)"
        case 2: return "v\(value)"
        case 3: return "0x" + String(value, radix: 16).uppercased()
        default: return "—"
        }
    }

    var expression: String {
        if opcode == 10 { return "No state change" }
        if opcode == 11 { return "Terminate shader" }
        let destinationName = "\(opcode >= 7 ? "v" : "s")\(destination)"
        let lhs = source(source0Kind, source0Value)
        switch opcode {
        case 3: return "\(destinationName) = NOT(\(lhs))"
        case 4, 5, 6, 8, 9:
            let operation: String
            switch opcode {
            case 4, 8: operation = "+"
            case 5: operation = "−"
            case 6: operation = "AND"
            default: operation = "×"
            }
            return "\(destinationName) = \(lhs) \(operation) \(source(source1Kind, source1Value))"
        default: return "\(destinationName) = \(lhs)"
        }
    }
}

struct NativeShaderIRSnapshot {
    let status: Int32
    let abiVersion: UInt32
    let pm4Packets: UInt32
    let pm4Draws: UInt32
    let irInstructions: UInt32
    let scalarInstructions: UInt32
    let vectorInstructions: UInt32
    let executedInstructions: UInt32
    let scalarWrites: UInt32
    let vectorWrites: UInt32
    let terminated: Bool
    let shaderReadOnly: Bool
    let descriptorReadOnly: Bool
    let sgpr1: UInt32
    let sgpr2: UInt32
    let sgpr3: UInt32
    let vgpr0Bits: UInt32
    let vgpr1Bits: UInt32
    let vgpr2Bits: UInt32
    let shaderAddress: UInt64
    let sourceChecksum: UInt64
    let irChecksum: UInt64
    let resultChecksum: UInt64
    let operations: [NativeShaderIROperation]

    var passed: Bool {
        status == 0 && abiVersion == 1 && pm4Packets == 11 && pm4Draws == 1 &&
        irInstructions == 8 && scalarInstructions == 5 && vectorInstructions == 3 &&
        executedInstructions == 8 && scalarWrites == 3 && vectorWrites == 3 &&
        terminated && shaderReadOnly && descriptorReadOnly &&
        sgpr1 == 7 && sgpr2 == 7 && sgpr3 == 14 &&
        vgpr0Bits == 0x40000000 && vgpr1Bits == 0x40800000 &&
        vgpr2Bits == 0x40800000 && shaderAddress == 0x100000 &&
        sourceChecksum != 0 && irChecksum != 0 && resultChecksum != 0 &&
        operations.count == 8 && operations.map(\.opcode) == [1, 2, 4, 7, 8, 9, 10, 11] &&
        operations[5].source0Kind == 3 && operations[5].source0Value == 0x3F800000
    }
}

enum NativeShaderIRAPI {
    static func runDiagnostic() -> NativeShaderIRSnapshot {
        let capacity = 256
        var buffer = [MisakiShaderIRInstruction](
            repeating: MisakiShaderIRInstruction(), count: capacity)
        var report = MisakiShaderIRReport()
        let status: Int32 = buffer.withUnsafeMutableBufferPointer { pointer in
            misaki_shader_ir_demo(pointer.baseAddress, pointer.count, &report)
        }
        let count = status == 0 && report.ir_instructions <= UInt32(capacity)
            ? Int(report.ir_instructions) : 0
        let operations = buffer.prefix(count).enumerated().map { index, ir in
            NativeShaderIROperation(
                id: index, guestAddress: ir.guest_address, wordOffset: ir.word_offset,
                opcode: ir.op, destination: ir.destination,
                source0Kind: ir.src0_kind, source0Value: ir.src0_value,
                source1Kind: ir.src1_kind, source1Value: ir.src1_value)
        }
        return NativeShaderIRSnapshot(
            status: status, abiVersion: report.abi_version,
            pm4Packets: report.pm4_packets, pm4Draws: report.pm4_draws,
            irInstructions: report.ir_instructions,
            scalarInstructions: report.scalar_instructions,
            vectorInstructions: report.vector_instructions,
            executedInstructions: report.executed_instructions,
            scalarWrites: report.scalar_writes, vectorWrites: report.vector_writes,
            terminated: report.terminated == 1,
            shaderReadOnly: report.source_read_only == 1,
            descriptorReadOnly: report.descriptor_read_only == 1,
            sgpr1: report.sgpr1, sgpr2: report.sgpr2, sgpr3: report.sgpr3,
            vgpr0Bits: report.vgpr0_bits, vgpr1Bits: report.vgpr1_bits,
            vgpr2Bits: report.vgpr2_bits,
            shaderAddress: report.shader_address,
            sourceChecksum: report.source_checksum,
            irChecksum: report.ir_checksum, resultChecksum: report.result_checksum,
            operations: operations)
    }
}
