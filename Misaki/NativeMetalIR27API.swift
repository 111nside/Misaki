import Foundation
import Metal

/// Generated from the supported subset of the GCN IR, never arbitrary guest
/// MSL source or a full PS4 shader. Expected results come from C++ evaluation.
struct NativeMetalIR27Plan {
    let status: Int32
    let abiVersion: UInt32
    let instructions: UInt32
    let statements: UInt32
    let sourceBytes: UInt32
    let irChecksum: UInt64
    let metalHash: UInt64
    let softwareHash: UInt64
    let expected: [UInt32]
    let source: String

    var passed: Bool {
        status == 0 && abiVersion == 1 && instructions == 8 && statements == 6 &&
        Int(sourceBytes) == source.utf8.count + 1 && sourceBytes <= 32768 &&
        irChecksum != 0 && metalHash != 0 && softwareHash != 0 &&
        expected == [7, 7, 14, 0x40000000, 0x40800000, 0x40800000] &&
        source.contains("kernel void misaki_ir27")
    }
}

enum NativeMetalIR27API {
    static func generate() -> NativeMetalIR27Plan {
        let capacity = 32768
        var bytes = [CChar](repeating: 0, count: capacity)
        var report = MisakiMetalIR27Report()
        let status: Int32 = bytes.withUnsafeMutableBufferPointer { ptr in
            misaki_metal_ir27_shader(ptr.baseAddress, ptr.count, &report)
        }
        var source = ""
        if status == 0 && report.source_bytes > 1 && report.source_bytes <= UInt32(capacity) &&
            bytes[Int(report.source_bytes) - 1] == 0 {
            source = bytes.withUnsafeBufferPointer { ptr in
                String(cString: ptr.baseAddress!)
            }
        }
        return NativeMetalIR27Plan(
            status: status, abiVersion: report.abi_version,
            instructions: report.ir_instructions, statements: report.alu_statements,
            sourceBytes: report.source_bytes, irChecksum: report.ir_checksum,
            metalHash: report.metal_source_hash,
            softwareHash: report.software_result_checksum,
            expected: [report.sgpr1, report.sgpr2, report.sgpr3,
                       report.vgpr0_bits, report.vgpr1_bits, report.vgpr2_bits],
            source: source)
    }
}

struct MetalIR27Execution: Sendable {
    let didRun: Bool
    let matched: Bool
    let received: [UInt32]
    let message: String

    static func failure(_ message: String) -> MetalIR27Execution {
        MetalIR27Execution(didRun: false, matched: false, received: [], message: message)
    }
}

/// Compiles the generated MSL into an iOS Metal COMPUTE pipeline, runs one
/// GPU work item, waits for completion, and checks bit-exact results against
/// the independently evaluated software IR. This is not a PS4 render path.
enum MetalIR27Runner {
    static func execute(source: String, expected: [UInt32]) -> MetalIR27Execution {
        guard !source.isEmpty && expected.count == 6 else {
            return .failure("Invalid shader source or reference output")
        }
        guard let device = MTLCreateSystemDefaultDevice() else {
            return .failure("Metal GPU is unavailable on this device")
        }
        do {
            let options = MTLCompileOptions()
            options.fastMathEnabled = false
            let library = try device.makeLibrary(source: source, options: options)
            guard let function = library.makeFunction(name: "misaki_ir27") else {
                return .failure("Compiled shader has no misaki_ir27 entry point")
            }
            let pipeline = try device.makeComputePipelineState(function: function)
            guard let queue = device.makeCommandQueue(),
                  let output = device.makeBuffer(length: 6 * MemoryLayout<UInt32>.stride,
                                                 options: .storageModeShared),
                  let buffer = queue.makeCommandBuffer(),
                  let encoder = buffer.makeComputeCommandEncoder() else {
                return .failure("Unable to allocate a Metal compute command")
            }
            encoder.setComputePipelineState(pipeline)
            encoder.setBuffer(output, offset: 0, index: 0)
            encoder.dispatchThreads(MTLSize(width: 1, height: 1, depth: 1),
                                    threadsPerThreadgroup: MTLSize(width: 1, height: 1, depth: 1))
            encoder.endEncoding()
            buffer.commit()
            buffer.waitUntilCompleted()
            guard buffer.status == .completed else {
                return .failure("Metal execution failed: \(buffer.error?.localizedDescription ?? "unknown GPU error")")
            }
            let pointer = output.contents().bindMemory(to: UInt32.self, capacity: 6)
            let result = (0..<6).map { pointer[$0] }
            return MetalIR27Execution(
                didRun: true, matched: result == expected, received: result,
                message: result == expected
                    ? "Six GPU register results exactly match C++ software evaluation"
                    : "GPU results differ from the C++ software reference")
        } catch {
            return .failure("Metal shader compilation or pipeline failed: \(error.localizedDescription)")
        }
    }
}
