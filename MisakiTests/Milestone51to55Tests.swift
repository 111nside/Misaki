import XCTest
@testable import Misaki

final class Milestone51to55Tests: XCTestCase {
    func testCompatibilityStagesAndExplicitBootBlockers() {
        let s = NativeMega55API.run()
        XCTAssertTrue(s.passed)
        XCTAssertEqual(s.status, 0)
        XCTAssertEqual(s.stages, [true, true, true, true, true])
        XCTAssertEqual(s.elfSegments, 2)
        XCTAssertEqual(s.elfRelocations, 1)
        XCTAssertEqual(s.selfEncryptedSegments, 1)
        XCTAssertTrue(s.encryptedSelfRefused)
        XCTAssertEqual(s.dynamicTags, 3)
        XCTAssertEqual(s.syntheticNids, 2)
        XCTAssertEqual(s.processes, 1)
        XCTAssertEqual(s.mappedPages, 2)
        XCTAssertEqual(s.pm4Packets, 11)
        XCTAssertEqual(s.gcnLanes, 4)
        XCTAssertEqual(s.gcnInstructions, 44)
        XCTAssertEqual(s.outputRecord7, 31)
        XCTAssertEqual(s.guestRax, 42)
        XCTAssertEqual(s.firmwareBlockers, 5)
        XCTAssertFalse(s.firmwareAttempted)
        XCTAssertFalse(s.homeMenuBooted)
    }

    func testGCNOutputDeterministicAcrossDiagnosticRuns() {
        let a = NativeMega55API.run()
        let b = NativeMega55API.run()
        XCTAssertTrue(a.passed)
        XCTAssertTrue(b.passed)
        XCTAssertNotEqual(a.shaderHash, 0)
        XCTAssertEqual(a.shaderHash, b.shaderHash)
    }
}
