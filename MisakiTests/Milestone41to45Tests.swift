import XCTest
@testable import Misaki

final class Milestone41to45Tests: XCTestCase {
    func testAllFiveSubsystemsAndCorrectlyBlockedFirmware() {
        let result = NativePS4PhaseTwoAPI.run()
        XCTAssertTrue(result.passed)
        XCTAssertEqual(result.status, 0)
        XCTAssertEqual(result.abiVersion, 1)
        XCTAssertEqual(result.stages, [true, true, true, true, true])
        XCTAssertEqual(result.segments, 1)
        XCTAssertEqual(result.relocations, 1)
        XCTAssertEqual(result.imports, 2)
        XCTAssertEqual(result.serviceCalls, 3)
        XCTAssertGreaterThan(result.guestInstructions, 0)
        XCTAssertEqual(result.guestRAX, 42)
        XCTAssertEqual(result.pm4Packets, 11)
        XCTAssertEqual(result.gpuPackets, 10)
        XCTAssertEqual(result.sprites, 2)
        XCTAssertEqual(result.fence, 1)
        XCTAssertEqual(result.realBootBlockers, 5)
        XCTAssertFalse(result.firmwareAttempted)
        XCTAssertFalse(result.homeMenuBooted)
    }

    func testDeterministicSyntheticIntegration() {
        let a = NativePS4PhaseTwoAPI.run()
        let b = NativePS4PhaseTwoAPI.run()
        XCTAssertTrue(a.passed)
        XCTAssertTrue(b.passed)
        XCTAssertEqual(a.framebufferChecksum, b.framebufferChecksum)
        XCTAssertEqual(a.guestInstructions, b.guestInstructions)
        XCTAssertNotEqual(a.framebufferChecksum, 0)
    }
}
