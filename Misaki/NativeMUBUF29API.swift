import Foundation

/// AMD GCN1.1 BUFFER_LOAD_DWORD / BUFFER_STORE_DWORD decoding and a
/// one-lane, descriptor-backed Metal compute diagnostic. Not a PS4 shader.
struct NativeMUBUF29Plan {
    let status: Int32
    let abiVersion: UInt32
    let alu: NativeMetalIR27Plan
    let source: String
    let sourceBytes: UInt32
    let inputBytes: [UInt8]
    let outputInitial: [UInt32]
    let outputExpected: [UInt32]
    let instructionCount: UInt32
    let loadOpcode: UInt32
    let storeOpcode: UInt32
    let vaddr: UInt32
    let vdata: UInt32
    let inputResourceSGPR: UInt32
    let outputResourceSGPR: UInt32
    let index: UInt32
    let loaded: UInt32
    let stored: UInt32
    let inputBase: UInt64
    let outputBase: UInt64
    let instructionHash: UInt64
    let sourceHash: UInt64
    let resultHash: UInt64
    let codeReadOnly: Bool
    let descriptorsReadOnly: Bool
    let inputReadOnly: Bool
    let outputWritable: Bool

    var passed: Bool {
        status == 0 && abiVersion == 1 && alu.passed &&
        instructionCount == 2 && loadOpcode == 12 && storeOpcode == 28 &&
        vaddr == 4 && vdata == 8 &&
        inputResourceSGPR == 0 && outputResourceSGPR == 4 &&
        index == 7 && loaded == 31 && stored == 31 &&
        inputBase == 0x400000 && outputBase == 0x500000 &&
        sourceBytes > 1 && sourceBytes <= 4096 &&
        source.utf8.count + 1 == Int(sourceBytes) &&
        source.contains("kernel void misaki_mubuf29") &&
        inputBytes.count == 1024 && outputInitial.count == 64 &&
        outputExpected.count == 64 && outputExpected[7] == 31 &&
        outputInitial[7] != outputExpected[7] &&
        zip(outputInitial, outputExpected).enumerated().allSatisfy { index, record in
            index == 7 || record.0 == record.1
        } &&
        instructionHash != 0 && sourceHash != 0 && resultHash != 0 &&
        codeReadOnly && descriptorsReadOnly && inputReadOnly && outputWritable
    }
}

enum NativeMUBUF29API {
    static func makePlan() -> NativeMUBUF29Plan {
        let alu = NativeMetalIR27API.generate()
        var source = [CChar](repeating: 0, count: 4096)
        var input = [UInt8](repeating: 0, count: 1024)
        var initial = [UInt32](repeating: 0, count: 64)
        var expected = [UInt32](repeating: 0, count: 64)
        var report = MisakiMUBUF29Report()
        let status: Int32 = source.withUnsafeMutableBufferPointer { shader in
            input.withUnsafeMutableBufferPointer { bytes in
                initial.withUnsafeMutableBufferPointer { start in
                    expected.withUnsafeMutableBufferPointer { end in
                        misaki_mubuf29_plan(
                            shader.baseAddress, shader.count,
                            bytes.baseAddress, bytes.count,
                            start.baseAddress, start.count,
                            end.baseAddress, end.count, &report)
                    }
                }
            }
        }
        var msl = ""
        if status == 0 && report.source_bytes > 1 &&
           report.source_bytes <= UInt32(source.count) &&
           source[Int(report.source_bytes) - 1] == 0 {
            msl = source.withUnsafeBufferPointer { String(cString: $0.baseAddress!) }
        }
        return NativeMUBUF29Plan(
            status: status, abiVersion: report.abi_version, alu: alu,
            source: msl, sourceBytes: report.source_bytes,
            inputBytes: input, outputInitial: initial, outputExpected: expected,
            instructionCount: report.decoded_instructions,
            loadOpcode: report.load_opcode, storeOpcode: report.store_opcode,
            vaddr: report.first_vaddr, vdata: report.first_vdata,
            inputResourceSGPR: report.input_srsrc,
            outputResourceSGPR: report.output_srsrc,
            index: report.record_index, loaded: report.loaded_word,
            stored: report.stored_word, inputBase: report.input_base,
            outputBase: report.output_base,
            instructionHash: report.instruction_checksum,
            sourceHash: report.metal_source_hash,
            resultHash: report.expected_output_hash,
            codeReadOnly: report.code_read_only == 1,
            descriptorsReadOnly: report.descriptors_read_only == 1,
            inputReadOnly: report.input_read_only == 1,
            outputWritable: report.output_writable == 1)
    }

    static func execute(_ plan: NativeMUBUF29Plan) -> MetalResource28Execution {
        guard plan.passed else {
            return .failure("MUBUF instruction or resource validation failed")
        }
        // Reuse the proven two-kernel execution machinery from Milestone 28.
        // The second kernel now comes from actual decoded GCN1.1 MUBUF opcodes.
        return MetalResource28Runner.execute(
            aluSource: plan.alu.source, resourceSource: plan.source,
            inputBytes: plan.inputBytes, initialOutput: plan.outputInitial,
            expectedRegisters: plan.alu.expected,
            expectedOutput: plan.outputExpected,
            resourceEntry: "misaki_mubuf29")
    }
}
