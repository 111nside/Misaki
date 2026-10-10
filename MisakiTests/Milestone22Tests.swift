import XCTest
@testable import Misaki

final class Milestone22Tests: XCTestCase {
    func testPM4GuestMemoryDemoDecodesActualPacketHeaders() {
        let result = NativePM4API.runDiagnostic()
        XCTAssertTrue(result.passed)
        XCTAssertEqual(result.status, 0)
        XCTAssertEqual(result.abiVersion, 1)
        XCTAssertEqual(result.packets, 9)
        XCTAssertEqual(result.type0Packets, 0)
        XCTAssertEqual(result.type2Packets, 1)
        XCTAssertEqual(result.type3Packets, 8)
        XCTAssertEqual(result.registerWrites, 4)
        XCTAssertEqual(result.contextWrites, 2)
        XCTAssertEqual(result.shaderWrites, 1)
        XCTAssertEqual(result.otherWrites, 1)
        XCTAssertEqual(result.draws, 1)
        XCTAssertEqual(result.lastVertexCount, 3)
        XCTAssertTrue(result.guestMemoryVerified)
        XCTAssertNotEqual(result.checksum, 0)
        XCTAssertEqual(result.registers.map(\.address), [0xA0B4, 0xA0B5, 0x2C0C, 0xC002])
    }

    func testPM4TraceIsDeterministic() {
        let a = NativePM4API.runDiagnostic()
        let b = NativePM4API.runDiagnostic()
        XCTAssertEqual(a.checksum, b.checksum)
        XCTAssertEqual(a.registers.map(\.value), b.registers.map(\.value))
        XCTAssertTrue(a.passed)
        XCTAssertTrue(b.passed)
    }
}
