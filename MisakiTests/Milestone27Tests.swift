import XCTest
import Metal
@testable import Misaki

final class Milestone27Tests: XCTestCase {
    func testNativeIRGeneratesMetalSource() {
        let result = NativeMetalIR27API.generate()
        XCTAssertTrue(result.passed)
        XCTAssertEqual(result.status, 0)
        XCTAssertEqual(result.instructions, 8)
        XCTAssertEqual(result.statements, 6)
        XCTAssertEqual(result.expected, [7, 7, 14, 0x40000000, 0x40800000, 0x40800000])
        XCTAssertTrue(result.source.contains("kernel void misaki_ir27"))
        XCTAssertTrue(result.source.contains("as_type<float>"))
        XCTAssertEqual(Int(result.sourceBytes), result.source.utf8.count + 1)
    }

    func testSourceAndReferenceAreDeterministic() {
        let a = NativeMetalIR27API.generate()
        let b = NativeMetalIR27API.generate()
        XCTAssertTrue(a.passed)
        XCTAssertEqual(a.source, b.source)
        XCTAssertEqual(a.irChecksum, b.irChecksum)
        XCTAssertEqual(a.metalHash, b.metalHash)
        XCTAssertEqual(a.softwareHash, b.softwareHash)
    }

    func testActualMetalComputesSixMatchingRegisterValuesWhenAvailable() throws {
        guard MTLCreateSystemDefaultDevice() != nil else {
            throw XCTSkip("Hosted simulator has no Metal device")
        }
        let plan = NativeMetalIR27API.generate()
        XCTAssertTrue(plan.passed)
        let gpu = MetalIR27Runner.execute(source: plan.source, expected: plan.expected)
        XCTAssertTrue(gpu.didRun, gpu.message)
        XCTAssertTrue(gpu.matched, gpu.message)
        XCTAssertEqual(gpu.received, plan.expected)
    }
}
