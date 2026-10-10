import XCTest
@testable import Misaki

final class Milestone25Tests: XCTestCase {
    func testGCNShaderGuestMemoryAndPM4Bridge() {
        let report = NativeGCN25API.runDiagnostic()
        XCTAssertTrue(report.passed)
        XCTAssertEqual(report.status, 0)
        XCTAssertEqual(report.abiVersion, 1)
        XCTAssertEqual(report.pm4Packets, 11)
        XCTAssertEqual(report.pm4Draws, 1)
        XCTAssertEqual(report.shaderAddress, 0x100000)
        XCTAssertEqual(report.instructionCount, 8)
        XCTAssertEqual(report.scalarCount, 5)
        XCTAssertEqual(report.vectorCount, 3)
        XCTAssertEqual(report.literalCount, 1)
        XCTAssertTrue(report.terminated)
        XCTAssertTrue(report.shaderReadOnly)
        XCTAssertTrue(report.descriptorReadOnly)
        XCTAssertEqual(report.descriptorStride, 16)
        XCTAssertEqual(report.descriptorRecords, 64)
        XCTAssertEqual(report.descriptorBase, 0x400000)
        XCTAssertEqual(report.instructions.map(\.mnemonic), [
            "s_movk_i32", "s_mov_b32", "s_add_u32", "v_mov_b32",
            "v_add_f32", "v_mul_f32", "s_nop", "s_endpgm"
        ])
    }

    func testGCNInstructionOffsetsAndDeterminism() {
        let a = NativeGCN25API.runDiagnostic()
        let b = NativeGCN25API.runDiagnostic()
        XCTAssertEqual(a.checksum, b.checksum)
        XCTAssertNotEqual(a.checksum, 0)
        XCTAssertEqual(a.instructions.map(\.wordOffset), [0, 1, 2, 3, 4, 5, 7, 8])
        XCTAssertEqual(a.instructions[5].wordLength, 2)
        XCTAssertTrue(a.instructions[5].hasLiteral)
        XCTAssertEqual(a.instructions[5].literal, 0x3F800000)
        XCTAssertEqual(a.instructions[7].guestAddress, 0x100020)
    }
}
