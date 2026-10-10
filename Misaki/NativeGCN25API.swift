import Foundation

/// A strict read-only view of AMD GCN *instruction encodings*, not an interpreter.
struct NativeGCN25Instruction: Identifiable {
    let id: Int
    let guestAddress: UInt64
    let wordOffset: UInt32
    let encoding: UInt32
    let opcode: UInt32
    let destination: UInt32
    let source0: UInt32
    let source1: UInt32
    let literal: UInt32
    let wordLength: UInt32
    let hasLiteral: Bool

    var family: String {
        switch encoding {
        case 1: return "SOP2"
        case 2: return "SOP1"
        case 3: return "SOPK"
        case 4: return "SOPP"
        case 5: return "VOP2"
        case 6: return "VOP1"
        default: return "Unknown"
        }
    }

    var mnemonic: String {
        switch (encoding, opcode) {
        case (1, 0): return "s_add_u32"
        case (1, 1): return "s_sub_u32"
        case (1, 12): return "s_and_b32"
        case (2, 0): return "s_mov_b32"
        case (2, 4): return "s_not_b32"
        case (3, 0): return "s_movk_i32"
        case (4, 0): return "s_nop"
        case (4, 1): return "s_endpgm"
        case (5, 1): return "v_add_f32"
        case (5, 5): return "v_mul_f32"
        case (6, 1): return "v_mov_b32"
        default: return "unknown_\(encoding)_\(opcode)"
        }
    }

    var operandSummary: String {
        if encoding == 4 { return "SIMM16=0x\(String(source0, radix: 16))" }
        if encoding == 3 { return "s\(destination), SIMM16=\(Int(Int16(bitPattern: UInt16(truncatingIfNeeded: source0))))" }
        if encoding == 6 { return "v\(destination), SRC0=\(source0)" }
        if encoding == 5 { return "v\(destination), SRC0=\(source0), VSRC1=v\(source1)" }
        if encoding == 2 { return "s\(destination), SRC0=\(source0)" }
        return "s\(destination), SRC0=\(source0), SRC1=\(source1)"
    }
}

struct NativeGCN25Snapshot {
    let status: Int32
    let abiVersion: UInt32
    let pm4Packets: UInt32
    let pm4Draws: UInt32
    let instructionCount: UInt32
    let scalarCount: UInt32
    let vectorCount: UInt32
    let literalCount: UInt32
    let shaderWords: UInt32
    let terminated: Bool
    let shaderReadOnly: Bool
    let descriptorReadOnly: Bool
    let descriptorSupported: Bool
    let descriptorStride: UInt32
    let descriptorRecords: UInt32
    let descriptorDataFormat: UInt32
    let descriptorNumericFormat: UInt32
    let shaderAddress: UInt64
    let descriptorBase: UInt64
    let checksum: UInt64
    let instructions: [NativeGCN25Instruction]

    var passed: Bool {
        status == 0 && abiVersion == 1 && pm4Packets == 11 && pm4Draws == 1 &&
        instructionCount == 8 && scalarCount == 5 && vectorCount == 3 &&
        literalCount == 1 && shaderWords == 9 && terminated && shaderReadOnly &&
        descriptorReadOnly && descriptorSupported && descriptorStride == 16 &&
        descriptorRecords == 64 && descriptorDataFormat == 14 &&
        descriptorNumericFormat == 0 && shaderAddress == 0x100000 &&
        descriptorBase == 0x400000 && checksum != 0 && instructions.count == 8 &&
        instructions.first?.mnemonic == "s_movk_i32" &&
        instructions.last?.mnemonic == "s_endpgm" &&
        instructions[5].hasLiteral && instructions[5].literal == 0x3F800000
    }
}

enum NativeGCN25API {
    static func runDiagnostic() -> NativeGCN25Snapshot {
        let capacity = 32
        var buffer = [MisakiGCN25Instruction](
            repeating: MisakiGCN25Instruction(), count: capacity)
        var report = MisakiGCN25Report()
        let status: Int32 = buffer.withUnsafeMutableBufferPointer { pointer in
            misaki_gcn25_diagnostic(pointer.baseAddress, pointer.count, &report)
        }
        let count = status == 0 && report.instruction_count <= UInt32(capacity)
            ? Int(report.instruction_count) : 0
        let instructions = buffer.prefix(count).enumerated().map { index, ins in
            NativeGCN25Instruction(
                id: index, guestAddress: ins.guest_address,
                wordOffset: ins.word_offset, encoding: ins.encoding,
                opcode: ins.opcode, destination: ins.dst,
                source0: ins.src0, source1: ins.src1,
                literal: ins.literal, wordLength: ins.length_words,
                hasLiteral: ins.has_literal == 1)
        }
        return NativeGCN25Snapshot(
            status: status, abiVersion: report.abi_version,
            pm4Packets: report.pm4_packets, pm4Draws: report.pm4_draws,
            instructionCount: report.instruction_count,
            scalarCount: report.scalar_instructions,
            vectorCount: report.vector_instructions,
            literalCount: report.literal_words,
            shaderWords: report.shader_words,
            terminated: report.terminated == 1,
            shaderReadOnly: report.shader_read_only == 1,
            descriptorReadOnly: report.descriptor_read_only == 1,
            descriptorSupported: report.descriptor_supported == 1,
            descriptorStride: report.descriptor_stride,
            descriptorRecords: report.descriptor_records,
            descriptorDataFormat: report.descriptor_data_format,
            descriptorNumericFormat: report.descriptor_numeric_format,
            shaderAddress: report.shader_address,
            descriptorBase: report.descriptor_base,
            checksum: report.checksum,
            instructions: instructions)
    }
}
