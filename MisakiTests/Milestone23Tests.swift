import XCTest
@testable import Misaki

final class Milestone23Tests: XCTestCase {
    func testNestedPM4DecodingFromGuestMemory() {
        let result = NativePM423API.runDiagnostic()
        XCTAssertTrue(result.passed)
        XCTAssertEqual(result.status, 0)
        XCTAssertEqual(result.abiVersion, 1)
        XCTAssertEqual(result.packetCount, 11)
        XCTAssertEqual(result.indirectBuffers, 2)
        XCTAssertEqual(result.indirectConstBuffers, 1)
        XCTAssertEqual(result.maximumDepth, 2)
        XCTAssertEqual(result.guestWords, 34)
        XCTAssertEqual(result.registerWrites, 8)
        XCTAssertEqual(result.drawPackets, 1)
        XCTAssertEqual(result.eventPackets, 1)
        XCTAssertTrue(result.targetMetadataObserved)
        XCTAssertEqual(result.observedRegisterMask, 255)
        XCTAssertEqual(result.colorBase, 0x200000)
        XCTAssertEqual(result.colorPitch, 127)
        XCTAssertEqual(result.colorInfo, 0x1B)
        XCTAssertEqual(result.targetMask, 15)
        XCTAssertEqual(result.pixelShaderLow, 0x1000)
        XCTAssertEqual(result.packets.count, 11)
        XCTAssertEqual(result.packets[2].opcode, 0x3F)
        XCTAssertEqual(result.packets[5].opcode, 0x33)
        XCTAssertEqual(result.packets[6].depth, 2)
        XCTAssertEqual(result.packets[9].depth, 1)
        XCTAssertEqual(result.packets[10].depth, 0)
    }

    func testNestedPM4DeterminismAndPacketOrdering() {
        let first = NativePM423API.runDiagnostic()
        let second = NativePM423API.runDiagnostic()
        XCTAssertTrue(first.passed)
        XCTAssertTrue(second.passed)
        XCTAssertEqual(first.checksum, second.checksum)
        XCTAssertEqual(first.packets.map(\.guestAddress), second.packets.map(\.guestAddress))
        XCTAssertEqual(first.packets.map(\.depth), [0, 0, 0, 1, 1, 1, 2, 2, 2, 1, 0])
    }
}
