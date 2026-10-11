import XCTest
import Metal
@testable import Misaki

final class Milestone30Tests: XCTestCase {
    func testNativeIntegratedGCNShaderAndMemory() {
        let plan = NativeGCN30API.prepare()
        XCTAssertTrue(plan.passed)
        XCTAssertEqual(plan.status, 0)
        XCTAssertEqual(plan.instructions, 11)
        XCTAssertEqual(plan.codeWords, 13)
        XCTAssertEqual(plan.aluOperations, 8)
        XCTAssertEqual(plan.loads, 1)
        XCTAssertEqual(plan.stores, 1)
        XCTAssertEqual(plan.executed, 11)
        XCTAssertEqual(plan.recordIndex, 7)
        XCTAssertEqual(plan.loaded, 31)
        XCTAssertEqual(plan.stored, 31)
        XCTAssertEqual(plan.expectedRegisters, [7, 7, 14, 25, 18, 7, 31, 31])
        XCTAssertEqual(plan.inputBytes.count, 1024)
        XCTAssertEqual(plan.outputInitial.count, 64)
        XCTAssertEqual(plan.outputExpected.count, 64)
        XCTAssertEqual(plan.outputExpected[7], 31)
        XCTAssertEqual(zip(plan.outputInitial, plan.outputExpected).filter { $0.0 != $0.1 }.count, 1)
        XCTAssertTrue(plan.codeReadOnly)
        XCTAssertTrue(plan.descriptorsReadOnly)
        XCTAssertTrue(plan.inputReadOnly)
        XCTAssertTrue(plan.outputWritable)
    }

    func testFusedKernelSourceIsDeterministicAndActuallyInterleaved() {
        let first = NativeGCN30API.prepare()
        let second = NativeGCN30API.prepare()
        XCTAssertTrue(first.passed)
        XCTAssertTrue(second.passed)
        XCTAssertEqual(first.programChecksum, second.programChecksum)
        XCTAssertEqual(first.sourceHash, second.sourceHash)
        XCTAssertEqual(first.outputHash, second.outputHash)
        XCTAssertEqual(first.source, second.source)
        XCTAssertEqual(first.outputExpected, second.outputExpected)
        XCTAssertTrue(first.source.contains("kernel void misaki_fused30"))
        guard let loadRange = first.source.range(of: "v[8] = inputWords["),
              let moveRange = first.source.range(of: "v[9] = v[8]"),
              let storeRange = first.source.range(of: "outputWords[") else {
            XCTFail("Fused shader is missing load/move/store")
            return
        }
        XCTAssertLessThan(loadRange.lowerBound, moveRange.lowerBound)
        XCTAssertLessThan(moveRange.lowerBound, storeRange.lowerBound)
    }

    func testActualMetalSingleKernelReadbackWhenAvailable() throws {
        guard MTLCreateSystemDefaultDevice() != nil else {
            throw XCTSkip("Simulator does not support Metal")
        }
        let plan = NativeGCN30API.prepare()
        XCTAssertTrue(plan.passed)
        let gpu = NativeGCN30API.execute(plan)
        XCTAssertTrue(gpu.didRun, gpu.message)
        XCTAssertTrue(gpu.matched, gpu.message)
        XCTAssertEqual(gpu.gpuStatus, 0)
        XCTAssertEqual(gpu.registers, plan.expectedRegisters)
        XCTAssertEqual(gpu.output, plan.outputExpected)
    }
}
