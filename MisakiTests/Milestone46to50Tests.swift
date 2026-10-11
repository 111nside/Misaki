import XCTest
@testable import Misaki

final class Milestone46to50Tests: XCTestCase {
    func testUnifiedGuestInitializationAndExplicitFirmwareBlockers() {
        let s = NativeMega50API.run()
        XCTAssertTrue(s.passed)
        XCTAssertEqual(s.status, 0)
        XCTAssertEqual(s.stages, [true, true, true, true, true])
        XCTAssertEqual(s.segments, 2)
        XCTAssertEqual(s.relocations, 1)
        XCTAssertEqual(s.modules, 2)
        XCTAssertEqual(s.imports, 2)
        XCTAssertEqual(s.relocationValue, 0x103500)
        XCTAssertEqual(s.guestRax, 42)
        XCTAssertEqual(s.vmPages, 1)
        XCTAssertEqual(s.serviceCalls, 4)
        XCTAssertEqual(s.serviceFailures, 0)
        XCTAssertEqual(s.pm4Packets, 11)
        XCTAssertEqual(s.sprites, 2)
        XCTAssertEqual(s.fence, 1)
        XCTAssertEqual(s.firmwareBlockers, 5)
        XCTAssertFalse(s.firmwareAttempted)
        XCTAssertFalse(s.homeMenuBooted)
    }

    func testIntegrationChecksumsAreDeterministic() {
        let a = NativeMega50API.run()
        let b = NativeMega50API.run()
        XCTAssertTrue(a.passed)
        XCTAssertTrue(b.passed)
        XCTAssertEqual(a.framebufferChecksum, b.framebufferChecksum)
        XCTAssertNotEqual(a.framebufferChecksum, 0)
    }
}
