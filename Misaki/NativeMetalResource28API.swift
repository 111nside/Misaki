import Foundation
import Metal

/// M28 is an isolated Metal resource-IO experiment. It reuses the verified
/// Milestone 27 translated ALU shader. The resource stage is a *synthetic*
/// three-operation IR, not decoded AMD GCN MUBUF instructions.
struct NativeMetalResource28Plan {
    let status: Int32
    let abiVersion: UInt32
    let alu: NativeMetalIR27Plan
    let source: String
    let sourceBytes: UInt32
    let inputBytes: [UInt8]
    let outputInitial: [UInt32]
    let outputExpected: [UInt32]
    let operations: UInt32
    let descriptorBase: UInt64
    let stride: UInt32
    let records: UInt32
    let readIndex: UInt32
    let writeIndex: UInt32
    let loadedWord: UInt32
    let storedWord: UInt32
    let sourceHash: UInt64
    let initialHash: UInt64
    let expectedHash: UInt64
    let irChecksum: UInt64
    let guestInputReadOnly: Bool
    let guestOutputWritable: Bool

    var passed: Bool {
        status == 0 && abiVersion == 1 && alu.passed &&
        operations == 3 && sourceBytes <= 4096 &&
        Int(sourceBytes) == source.utf8.count + 1 &&
        source.contains("kernel void misaki_resource28") &&
        inputBytes.count == 1024 && outputInitial.count == 64 &&
        outputExpected.count == 64 &&
        descriptorBase == 0x400000 && stride == 16 && records == 64 &&
        readIndex == 7 && writeIndex == 6 &&
        loadedWord == 31 && storedWord == 45 &&
        outputInitial[6] != outputExpected[6] && outputExpected[6] == 45 &&
        zip(outputInitial, outputExpected).enumerated().allSatisfy { index, item in
            index == 6 || item.0 == item.1
        } &&
        sourceHash != 0 && initialHash != 0 && expectedHash != 0 &&
        expectedHash != initialHash && irChecksum == alu.irChecksum &&
        guestInputReadOnly && guestOutputWritable
    }
}

enum NativeMetalResource28API {
    static func makePlan() -> NativeMetalResource28Plan {
        let alu = NativeMetalIR27API.generate()
        var source = [CChar](repeating: 0, count: 4096)
        var input = [UInt8](repeating: 0, count: 1024)
        var initial = [UInt32](repeating: 0, count: 64)
        var expected = [UInt32](repeating: 0, count: 64)
        var report = MisakiMetalResource28Report()
        let status: Int32 = source.withUnsafeMutableBufferPointer { src in
            input.withUnsafeMutableBufferPointer { guest in
                initial.withUnsafeMutableBufferPointer { output in
                    expected.withUnsafeMutableBufferPointer { reference in
                        misaki_metal_resource28_plan(
                            src.baseAddress, src.count,
                            guest.baseAddress, guest.count,
                            output.baseAddress, output.count,
                            reference.baseAddress, reference.count, &report)
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
        return NativeMetalResource28Plan(
            status: status, abiVersion: report.abi_version, alu: alu,
            source: msl, sourceBytes: report.source_bytes,
            inputBytes: input, outputInitial: initial, outputExpected: expected,
            operations: report.resource_ops, descriptorBase: report.descriptor_base,
            stride: report.descriptor_stride, records: report.descriptor_records,
            readIndex: report.read_index, writeIndex: report.write_index,
            loadedWord: report.loaded_word, storedWord: report.stored_word,
            sourceHash: report.metal_source_hash,
            initialHash: report.output_initial_hash,
            expectedHash: report.output_expected_hash,
            irChecksum: report.ir_checksum,
            guestInputReadOnly: report.guest_input_read_only == 1,
            guestOutputWritable: report.guest_output_writable == 1)
    }
}

struct MetalResource28Execution: Sendable {
    let didRun: Bool
    let matched: Bool
    let computedRegisters: [UInt32]
    let output: [UInt32]
    let gpuStatus: UInt32
    let message: String

    static func failure(_ message: String) -> MetalResource28Execution {
        MetalResource28Execution(didRun: false, matched: false,
                                 computedRegisters: [], output: [],
                                 gpuStatus: UInt32.max, message: message)
    }
}

enum MetalResource28Runner {
    /// Two ordered Metal compute dispatches. The first generates the six M27
    /// GCN-IR registers on the *GPU*, then the second uses that GPU buffer as
    /// its input when reading/writing resource memory. CPU code only compares.
    static func execute(aluSource: String, resourceSource: String,
                        inputBytes: [UInt8], initialOutput: [UInt32],
                        expectedRegisters: [UInt32],
                        expectedOutput: [UInt32]) -> MetalResource28Execution {
        guard !aluSource.isEmpty && !resourceSource.isEmpty &&
              inputBytes.count == 1024 && initialOutput.count == 64 &&
              expectedRegisters.count == 6 && expectedOutput.count == 64 else {
            return .failure("Invalid reference buffers or generated shader sources")
        }
        guard let device = MTLCreateSystemDefaultDevice() else {
            return .failure("Metal GPU unavailable")
        }
        do {
            let options = MTLCompileOptions()
            options.fastMathEnabled = false
            let aluLibrary = try device.makeLibrary(source: aluSource, options: options)
            let resourceLibrary = try device.makeLibrary(source: resourceSource, options: options)
            guard let aluFunction = aluLibrary.makeFunction(name: "misaki_ir27"),
                  let resourceFunction = resourceLibrary.makeFunction(name: "misaki_resource28") else {
                return .failure("Generated Metal kernels could not be found")
            }
            let aluPipeline = try device.makeComputePipelineState(function: aluFunction)
            let resourcePipeline = try device.makeComputePipelineState(function: resourceFunction)
            let gpuInput = inputBytes.withUnsafeBytes { data in
                device.makeBuffer(bytes: data.baseAddress!, length: data.count,
                                  options: .storageModeShared)
            }
            let gpuOutput = initialOutput.withUnsafeBytes { data in
                device.makeBuffer(bytes: data.baseAddress!, length: data.count,
                                  options: .storageModeShared)
            }
            guard let queue = device.makeCommandQueue(),
                  let registerBuffer = device.makeBuffer(
                      length: expectedRegisters.count * MemoryLayout<UInt32>.stride,
                      options: .storageModeShared),
                  let inputBuffer = gpuInput,
                  let outputBuffer = gpuOutput,
                  let statusBuffer = device.makeBuffer(length: 4, options: .storageModeShared),
                  let commandBuffer = queue.makeCommandBuffer() else {
                return .failure("Could not allocate Metal command or resource buffers")
            }
            // Sentinel prevents an unexecuted second kernel from looking successful.
            statusBuffer.contents().bindMemory(to: UInt32.self, capacity: 1).pointee = UInt32.max
            guard let aluEncoder = commandBuffer.makeComputeCommandEncoder() else {
                return .failure("Cannot create ALU compute encoder")
            }
            aluEncoder.setComputePipelineState(aluPipeline)
            aluEncoder.setBuffer(registerBuffer, offset: 0, index: 0)
            aluEncoder.dispatchThreads(MTLSize(width: 1, height: 1, depth: 1),
                                       threadsPerThreadgroup: MTLSize(width: 1, height: 1, depth: 1))
            aluEncoder.endEncoding()
            guard let resourceEncoder = commandBuffer.makeComputeCommandEncoder() else {
                return .failure("Cannot create resource compute encoder")
            }
            resourceEncoder.setComputePipelineState(resourcePipeline)
            resourceEncoder.setBuffer(registerBuffer, offset: 0, index: 0)
            resourceEncoder.setBuffer(inputBuffer, offset: 0, index: 1)
            resourceEncoder.setBuffer(outputBuffer, offset: 0, index: 2)
            resourceEncoder.setBuffer(statusBuffer, offset: 0, index: 3)
            resourceEncoder.dispatchThreads(MTLSize(width: 1, height: 1, depth: 1),
                                            threadsPerThreadgroup: MTLSize(width: 1, height: 1, depth: 1))
            resourceEncoder.endEncoding()
            commandBuffer.commit()
            commandBuffer.waitUntilCompleted()
            guard commandBuffer.status == .completed else {
                return .failure("Metal dispatch error: \(commandBuffer.error?.localizedDescription ?? "unknown")")
            }
            let registerPtr = registerBuffer.contents().bindMemory(to: UInt32.self, capacity: 6)
            let outputPtr = outputBuffer.contents().bindMemory(to: UInt32.self, capacity: 64)
            let statusWord = statusBuffer.contents().bindMemory(to: UInt32.self, capacity: 1).pointee
            let actualRegisters = (0..<6).map { registerPtr[$0] }
            let actualOutput = (0..<64).map { outputPtr[$0] }
            let matched = statusWord == 0 && actualRegisters == expectedRegisters &&
                          actualOutput == expectedOutput
            let changed = zip(initialOutput, actualOutput).filter { $0.0 != $0.1 }.count
            return MetalResource28Execution(
                didRun: true, matched: matched,
                computedRegisters: actualRegisters, output: actualOutput,
                gpuStatus: statusWord,
                message: matched
                    ? "Both GPU kernels ran; all 6 ALU registers and 64 resource records match the independent C++ reference"
                    : "Mismatch: \(changed) output records changed; GPU status=\(statusWord). Inspect register and output comparisons.")
        } catch {
            return .failure("Metal shader compilation or execution setup failed: \(error.localizedDescription)")
        }
    }
}
