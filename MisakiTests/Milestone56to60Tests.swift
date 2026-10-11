import XCTest
@testable import Misaki

final class Milestone56to60Tests: XCTestCase {
    func testFiveSubsystemDiagnostics() {
        let report = NativeMega60API.run()
        XCTAssertTrue(report.passed)
        XCTAssertEqual(report.status, 0)
        XCTAssertEqual(report.stages, [true, true, true, true, true])
        XCTAssertEqual(report.guestExits, 2)
        XCTAssertEqual(report.relocations, 1)
        XCTAssertFalse(report.firmwareAttempted)
        XCTAssertFalse(report.homeMenuBooted)
    }

    func testUnrecognizedFileIsNotExecuted() {
        let result = NativeMega60API.analyze(prefix: Data([0x41, 0x42, 0x43, 0x44]), totalSize: 4)
        XCTAssertEqual(result.status, 0)
        XCTAssertFalse(result.recognized)
        XCTAssertTrue(result.executionRefused)
        XCTAssertTrue(result.text.contains("Unrecognized"))
    }

    func testReportsPartialFileRead() {
        let result = NativeMega60API.analyze(prefix: Data([0x41, 0x42, 0x43, 0x44]), totalSize: 4096)
        XCTAssertEqual(result.status, 0)
        XCTAssertTrue(result.partial)
        XCTAssertTrue(result.executionRefused)
    }
}
