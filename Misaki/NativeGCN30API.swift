import Foundation
import Metal

/// Milestone 30: a single ordered shader containing decoded GCN1.1 ALU and
/// buffer-memory instructions. Executes one virtual lane only, not a PS4 game.
struct NativeGCN30Plan {
    let status: Int32
    let abiVersion: UInt32
    let source: String
    let sourceBytes: UInt32
    let inputBytes: [UInt8]
    let outputInitial: [UInt32]
    let outputExpected: [UInt32]
    let expectedRegisters: [UInt32]
    let instructions: UInt32
    let codeWords: UInt32
    let aluOperations: UInt32
    let loads: UInt32
    let stores: UInt32
    let executed: UInt32
    let recordIndex: UInt32
    let loaded: UInt32
    let stored: UInt32
    let inputBase: UInt64
    let outputBase: UInt64
    let programChecksum: UInt64
    let sourceHash: UInt64
    let outputHash: UInt64
    let codeReadOnly: Bool
    let descriptorsReadOnly: Bool
    let inputReadOnly: Bool
    let outputWritable: Bool

    var passed: Bool {
        status == 0 && abiVersion == 1 &&
        instructions == 11 && codeWords == 13 &&
        aluOperations == 8 && loads == 1 && stores == 1 && executed == 11 &&
        recordIndex == 7 && loaded == 31 && stored == 31 &&
        inputBase == 0x400000 && outputBase == 0x500000 &&
        sourceBytes > 1 && sourceBytes <= 8192 &&
        source.utf8.count + 1 == Int(sourceBytes) &&
        source.contains("kernel void misaki_fused30") &&
        source.contains("v[8] = inputWords[") &&
        source.contains("v[9] = v[8]") &&
        source.contains("outputWords[") &&
        inputBytes.count == 1024 && outputInitial.count == 64 &&
        outputExpected.count == 64 &&
        expectedRegisters == [7, 7, 14, 25, 18, 7, 31, 31] &&
        outputExpected[7] == 31 &&
        zip(outputInitial, outputExpected).enumerated().allSatisfy { index, pair in
            index == 7 ? pair.0 != pair.1 : pair.0 == pair.1
        } &&
        programChecksum != 0 && sourceHash != 0 && outputHash != 0 &&
        codeReadOnly && descriptorsReadOnly && inputReadOnly && outputWritable
    }
}

enum NativeGCN30API {
    static func prepare() -> NativeGCN30Plan {
        var shader = [CChar](repeating: 0, count: 8192)
        var input = [UInt8](repeating: 0, count: 1024)
        var initial = [UInt32](repeating: 0, count: 64)
        var expected = [UInt32](repeating: 0, count: 64)
        var registers = [UInt32](repeating: 0, count: 8)
        var info = MisakiFusedGCN30Report()
        let status: Int32 = shader.withUnsafeMutableBufferPointer { src in
            input.withUnsafeMutableBufferPointer { bytes in
                initial.withUnsafeMutableBufferPointer { before in
                    expected.withUnsafeMutableBufferPointer { after in
                        registers.withUnsafeMutableBufferPointer { regs in
                            misaki_gcn30_plan(src.baseAddress, src.count,
                                bytes.baseAddress, bytes.count,
                                before.baseAddress, before.count,
                                after.baseAddress, after.count,
                                regs.baseAddress, regs.count, &info)
                        }
                    }
                }
            }
        }
        var source = ""
        if status == 0 && info.source_bytes > 1 &&
           info.source_bytes <= UInt32(shader.count) &&
           shader[Int(info.source_bytes) - 1] == 0 {
            source = shader.withUnsafeBufferPointer { String(cString: $0.baseAddress!) }
        }
        return NativeGCN30Plan(
            status: status, abiVersion: info.abi_version, source: source,
            sourceBytes: info.source_bytes,
            inputBytes: input, outputInitial: initial,
            outputExpected: expected, expectedRegisters: registers,
            instructions: info.instructions, codeWords: info.code_words,
            aluOperations: info.alu_operations,
            loads: info.buffer_loads, stores: info.buffer_stores,
            executed: info.executed, recordIndex: info.record_index,
            loaded: info.loaded, stored: info.stored,
            inputBase: info.input_base, outputBase: info.output_base,
            programChecksum: info.instruction_checksum,
            sourceHash: info.metal_source_hash,
            outputHash: info.expected_output_hash,
            codeReadOnly: info.code_read_only == 1,
            descriptorsReadOnly: info.descriptors_read_only == 1,
            inputReadOnly: info.input_read_only == 1,
            outputWritable: info.output_writable == 1)
    }

    static func execute(_ plan: NativeGCN30Plan) -> MetalGCN30Result {
        guard plan.passed else { return .failure("Native fused shader plan failed validation") }
        guard let device = MTLCreateSystemDefaultDevice() else {
            return .failure("Metal is unavailable on this device")
        }
        do {
            let options = MTLCompileOptions()
            options.fastMathEnabled = false
            let library = try device.makeLibrary(source: plan.source, options: options)
            guard let function = library.makeFunction(name: "misaki_fused30") else {
                return .failure("Generated Metal kernel was not found")
            }
            let pipeline = try device.makeComputePipelineState(function: function)
            let inputBuffer = plan.inputBytes.withUnsafeBytes { bytes in
                device.makeBuffer(bytes: bytes.baseAddress!, length: bytes.count,
                                  options: .storageModeShared)
            }
            let outputBuffer = plan.outputInitial.withUnsafeBytes { bytes in
                device.makeBuffer(bytes: bytes.baseAddress!, length: bytes.count,
                                  options: .storageModeShared)
            }
            guard let inputBuffer, let outputBuffer,
                  let registerBuffer = device.makeBuffer(length: 8 * MemoryLayout<UInt32>.stride,
                                                         options: .storageModeShared),
                  let statusBuffer = device.makeBuffer(length: MemoryLayout<UInt32>.stride,
                                                       options: .storageModeShared),
                  let queue = device.makeCommandQueue(),
                  let commandBuffer = queue.makeCommandBuffer() else {
                return .failure("Metal GPU buffer or command allocation failed")
            }
            let registerPointer = registerBuffer.contents().bindMemory(to: UInt32.self, capacity: 8)
            for i in 0..<8 { registerPointer[i] = UInt32.max }
            statusBuffer.contents().bindMemory(to: UInt32.self, capacity: 1).pointee = UInt32.max
            guard let encoder = commandBuffer.makeComputeCommandEncoder() else {
                return .failure("Metal compute encoder creation failed")
            }
            encoder.setComputePipelineState(pipeline)
            encoder.setBuffer(inputBuffer, offset: 0, index: 0)
            encoder.setBuffer(outputBuffer, offset: 0, index: 1)
            encoder.setBuffer(registerBuffer, offset: 0, index: 2)
            encoder.setBuffer(statusBuffer, offset: 0, index: 3)
            encoder.dispatchThreads(MTLSize(width: 1, height: 1, depth: 1),
                                    threadsPerThreadgroup: MTLSize(width: 1, height: 1, depth: 1))
            encoder.endEncoding()
            commandBuffer.commit()
            commandBuffer.waitUntilCompleted()
            guard commandBuffer.status == .completed else {
                return .failure("Metal dispatch failed: \(commandBuffer.error?.localizedDescription ?? "unknown")")
            }
            let status = statusBuffer.contents().bindMemory(to: UInt32.self, capacity: 1).pointee
            let resultRegisters = (0..<8).map { registerPointer[$0] }
            let outputPointer = outputBuffer.contents().bindMemory(to: UInt32.self, capacity: 64)
            let output = (0..<64).map { outputPointer[$0] }
            let match = status == 0 && resultRegisters == plan.expectedRegisters &&
                        output == plan.outputExpected
            let changed = zip(plan.outputInitial, output).filter { $0.0 != $0.1 }.count
            return MetalGCN30Result(
                didRun: true, matched: match, gpuStatus: status,
                registers: resultRegisters, output: output,
                message: match
                    ? "One Metal kernel executed the interleaved ALU/load/move/store shader. All 8 registers and 64 memory records match the independent C++ reference."
                    : "Mismatch: GPU status=\(status), \(changed) output records changed. Check register and buffer results.")
        } catch {
            return .failure("Metal shader compilation or setup failed: \(error.localizedDescription)")
        }
    }
}

struct MetalGCN30Result: Sendable {
    let didRun: Bool
    let matched: Bool
    let gpuStatus: UInt32
    let registers: [UInt32]
    let output: [UInt32]
    let message: String

    static func failure(_ why: String) -> MetalGCN30Result {
        MetalGCN30Result(didRun: false, matched: false, gpuStatus: UInt32.max,
                         registers: [], output: [], message: why)
    }
}
