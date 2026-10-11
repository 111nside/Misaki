import XCTest
@testable import Misaki

final class Milestone26Tests: XCTestCase {
    func testGCNShaderIRCompilesAndEvaluatesSample() {
        let result = NativeShaderIRAPI.runDiagnostic()
        XCTAssertTrue(result.passed)
        XCTAssertEqual(result.status, 0)
        XCTAssertEqual(result.abiVersion, 1)
        XCTAssertEqual(result.pm4Packets, 11)
        XCTAssertEqual(result.pm4Draws, 1)
        XCTAssertEqual(result.irInstructions, 8)
        XCTAssertEqual(result.executedInstructions, 8)
        XCTAssertEqual(result.scalarWrites, 3)
        XCTAssertEqual(result.vectorWrites, 3)
        XCTAssertEqual([result.sgpr1, result.sgpr2, result.sgpr3], [7, 7, 14])
        XCTAssertEqual(Float(bitPattern: result.vgpr0Bits), 2.0)
        XCTAssertEqual(Float(bitPattern: result.vgpr1Bits), 4.0)
        XCTAssertEqual(Float(bitPattern: result.vgpr2Bits), 4.0)
        XCTAssertTrue(result.shaderReadOnly)
        XCTAssertTrue(result.descriptorReadOnly)
    }

    func testIRInstructionsAndChecksumsAreDeterministic() {
        let a = NativeShaderIRAPI.runDiagnostic()
        let b = NativeShaderIRAPI.runDiagnostic()
        XCTAssertTrue(a.passed)
        XCTAssertTrue(b.passed)
        XCTAssertEqual(a.operations.map(\.opcode), [1, 2, 4, 7, 8, 9, 10, 11])
        XCTAssertEqual(a.operations.map(\.opcode), b.operations.map(\.opcode))
        XCTAssertEqual(a.sourceChecksum, b.sourceChecksum)
        XCTAssertEqual(a.irChecksum, b.irChecksum)
        XCTAssertEqual(a.resultChecksum, b.resultChecksum)
        XCTAssertEqual(a.operations[5].source0Kind, 3)
        XCTAssertEqual(a.operations[5].source0Value, 0x3F800000)
        XCTAssertEqual(a.operations[3].expression, "v0 = v1")
        XCTAssertEqual(a.operations[7].mnemonic, "s_endpgm")
    }
}
