import XCTest
@testable import Misaki

final class Milestone36to40Tests: XCTestCase {
    func testEachCompatibilitySubsystem() {
        let r = NativePS4BootAPI.run()
        XCTAssertTrue(r.passed)
        XCTAssertEqual(r.stages, [true, true, true, true, true])
        XCTAssertEqual(r.selfSegments, 1)
        XCTAssertEqual(r.encryptedSegments, 1)
        XCTAssertEqual(r.sceELFType, 0xFE10)
        XCTAssertEqual(r.syntheticImports, 2)
        XCTAssertEqual(r.gpuLanes, 4)
        XCTAssertEqual(r.gcnInstructions, 44)
        XCTAssertEqual(r.processes, 2)
        XCTAssertEqual(r.pm4Packets, 11)
        XCTAssertEqual(r.fence, 1)
    }
    func testRealBootIsBlockedEvenWhenFixtureTestsPass() {
        let r = NativePS4BootAPI.run()
        XCTAssertTrue(r.passed)
        XCTAssertEqual(r.readinessBlockers, 5)
        XCTAssertFalse(r.firmwareAttempted)
        XCTAssertFalse(r.homeMenuBooted)
    }
    func testDeterministicBootInspection() {
        let a = NativePS4BootAPI.run()
        let b = NativePS4BootAPI.run()
        XCTAssertEqual(a.status, b.status)
        XCTAssertEqual(a.x64Instructions, b.x64Instructions)
        XCTAssertEqual(a.stages, b.stages)
    }
}
