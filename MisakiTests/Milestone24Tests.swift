import XCTest
@testable import Misaki

final class Milestone24Tests: XCTestCase {
    func testAMDPM4DrawTimeMetadata() throws {
        let result = NativePM424API.runDiagnostic()
        XCTAssertTrue(result.passed)
        XCTAssertEqual(result.status, 0)
        XCTAssertEqual(result.abiVersion, 1)
        XCTAssertEqual(result.packets, 11)
        XCTAssertEqual(result.indirectBuffers, 2)
        XCTAssertEqual(result.registerWrites, 8)
        XCTAssertEqual(result.drawCount, 1)
        XCTAssertEqual(result.readyDraws, 1)
        XCTAssertEqual(result.rejectedDraws, 0)
        let d = try XCTUnwrap(result.draws.first)
        XCTAssertEqual(d.packetAddress, 0x5002C)
        XCTAssertEqual(d.depth, 1)
        XCTAssertEqual(d.packetIndex, 9)
        XCTAssertEqual(d.vertices, 3)
        XCTAssertEqual(d.colorAddress, 0x20000000)
        XCTAssertEqual(d.pixelShaderAddress, 0x100000)
        XCTAssertEqual(d.scissorX, 10)
        XCTAssertEqual(d.scissorY, 10)
        XCTAssertEqual(d.scissorWidth, 190)
        XCTAssertEqual(d.scissorHeight, 110)
        XCTAssertEqual(d.pitchTileMax, 127)
        XCTAssertEqual(d.colorFormatField, 27)
        XCTAssertEqual(d.targetMask, 15)
        XCTAssertTrue(d.metadataComplete)
    }

    func testPM4StateAnalysisIsDeterministic() {
        let a = NativePM424API.runDiagnostic()
        let b = NativePM424API.runDiagnostic()
        XCTAssertEqual(a.checksum, b.checksum)
        XCTAssertEqual(a.draws.first?.packetAddress, b.draws.first?.packetAddress)
        XCTAssertTrue(a.passed && b.passed)
    }
}
