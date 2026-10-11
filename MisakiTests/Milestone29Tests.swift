import XCTest
import Metal
@testable import Misaki

final class Milestone29Tests: XCTestCase {
    func testGCN11MUBUFDecodesAndExecutesInGuestMemory() {
        let plan = NativeMUBUF29API.makePlan()
        XCTAssertTrue(plan.passed)
        XCTAssertEqual(plan.status, 0)
        XCTAssertEqual(plan.instructionCount, 2)
        XCTAssertEqual(plan.loadOpcode, 12)
        XCTAssertEqual(plan.storeOpcode, 28)
        XCTAssertEqual(plan.vaddr, 4)
        XCTAssertEqual(plan.vdata, 8)
        XCTAssertEqual(plan.inputResourceSGPR, 0)
        XCTAssertEqual(plan.outputResourceSGPR, 4)
        XCTAssertEqual(plan.index, 7)
        XCTAssertEqual(plan.loaded, 31)
        XCTAssertEqual(plan.stored, 31)
        XCTAssertEqual(plan.outputExpected[7], 31)
        XCTAssertEqual(plan.inputBytes.count, 1024)
        XCTAssertEqual(plan.outputExpected.count, 64)
        XCTAssertEqual(zip(plan.outputInitial, plan.outputExpected).filter { $0.0 != $0.1 }.count, 1)
        XCTAssertTrue(plan.codeReadOnly)
        XCTAssertTrue(plan.descriptorsReadOnly)
        XCTAssertTrue(plan.inputReadOnly)
        XCTAssertTrue(plan.outputWritable)
    }

    func testDecodedMUBUFMSLIsDeterministic() {
        let a = NativeMUBUF29API.makePlan()
        let b = NativeMUBUF29API.makePlan()
        XCTAssertTrue(a.passed)
        XCTAssertTrue(b.passed)
        XCTAssertEqual(a.instructionHash, b.instructionHash)
        XCTAssertEqual(a.sourceHash, b.sourceHash)
        XCTAssertEqual(a.resultHash, b.resultHash)
        XCTAssertEqual(a.source, b.source)
        XCTAssertEqual(a.outputExpected, b.outputExpected)
        XCTAssertTrue(a.source.contains("kernel void misaki_mubuf29"))
        XCTAssertTrue(a.source.contains("guest_v8"))
    }

    func testActualMetalMUBUFReadbackWhenAvailable() throws {
        guard MTLCreateSystemDefaultDevice() != nil else {
            throw XCTSkip("Simulator has no Metal GPU")
        }
        let plan = NativeMUBUF29API.makePlan()
        XCTAssertTrue(plan.passed)
        let gpu = NativeMUBUF29API.execute(plan)
        XCTAssertTrue(gpu.didRun, gpu.message)
        XCTAssertTrue(gpu.matched, gpu.message)
        XCTAssertEqual(gpu.gpuStatus, 0)
        XCTAssertEqual(gpu.computedRegisters, plan.alu.expected)
        XCTAssertEqual(gpu.output, plan.outputExpected)
    }
}
