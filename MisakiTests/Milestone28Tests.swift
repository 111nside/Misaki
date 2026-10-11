import XCTest
import Metal
@testable import Misaki

final class Milestone28Tests: XCTestCase {
    func testNativeResourceReferenceAndDescriptor() {
        let plan = NativeMetalResource28API.makePlan()
        XCTAssertTrue(plan.passed)
        XCTAssertEqual(plan.status, 0)
        XCTAssertEqual(plan.abiVersion, 1)
        XCTAssertEqual(plan.operations, 3)
        XCTAssertEqual(plan.descriptorBase, 0x400000)
        XCTAssertEqual(plan.stride, 16)
        XCTAssertEqual(plan.records, 64)
        XCTAssertEqual(plan.inputBytes.count, 1024)
        XCTAssertEqual(plan.readIndex, 7)
        XCTAssertEqual(plan.writeIndex, 6)
        XCTAssertEqual(plan.loadedWord, 31)
        XCTAssertEqual(plan.storedWord, 45)
        XCTAssertTrue(plan.guestInputReadOnly)
        XCTAssertTrue(plan.guestOutputWritable)
        XCTAssertEqual(plan.outputExpected[6], 45)
        XCTAssertEqual(zip(plan.outputInitial, plan.outputExpected).filter { $0.0 != $0.1 }.count, 1)
        XCTAssertEqual(plan.irChecksum, plan.alu.irChecksum)
    }

    func testResourceMSLAndReferenceAreDeterministic() {
        let a = NativeMetalResource28API.makePlan()
        let b = NativeMetalResource28API.makePlan()
        XCTAssertTrue(a.passed)
        XCTAssertTrue(b.passed)
        XCTAssertEqual(a.source, b.source)
        XCTAssertEqual(a.sourceHash, b.sourceHash)
        XCTAssertEqual(a.initialHash, b.initialHash)
        XCTAssertEqual(a.expectedHash, b.expectedHash)
        XCTAssertEqual(a.outputExpected, b.outputExpected)
        XCTAssertTrue(a.source.contains("guestWords[readIndex * 4u]"))
        XCTAssertTrue(a.source.contains("outputWords[writeIndex] = result"))
        XCTAssertTrue(a.source.contains("[[buffer(3)]]"))
    }

    func testActualMetalGPUResourceReadbackWhenAvailable() throws {
        guard MTLCreateSystemDefaultDevice() != nil else {
            throw XCTSkip("Hosted simulator has no Metal GPU")
        }
        let plan = NativeMetalResource28API.makePlan()
        XCTAssertTrue(plan.passed)
        let gpu = MetalResource28Runner.execute(
            aluSource: plan.alu.source, resourceSource: plan.source,
            inputBytes: plan.inputBytes, initialOutput: plan.outputInitial,
            expectedRegisters: plan.alu.expected,
            expectedOutput: plan.outputExpected)
        XCTAssertTrue(gpu.didRun, gpu.message)
        XCTAssertTrue(gpu.matched, gpu.message)
        XCTAssertEqual(gpu.gpuStatus, 0)
        XCTAssertEqual(gpu.computedRegisters, plan.alu.expected)
        XCTAssertEqual(gpu.output, plan.outputExpected)
    }
}
