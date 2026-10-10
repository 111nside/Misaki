import XCTest
@testable import Misaki

final class Milestone18Tests: XCTestCase {
    func testGuestGPUFrameBridge() {
        let frame = NativeGPUAPI.makeDemoFrame()
        XCTAssertTrue(frame.passed)
        XCTAssertEqual(frame.status, 0)
        XCTAssertEqual(frame.width, 320)
        XCTAssertEqual(frame.height, 180)
        XCTAssertEqual(frame.commandCount, 7)
        XCTAssertEqual(frame.rectangles, 5)
        XCTAssertEqual(frame.clears, 1)
        XCTAssertEqual(frame.presents, 1)
        XCTAssertNotEqual(frame.checksum, 0)
    }

    func testCommandsHaveDeterministicOrdering() {
        let a = NativeGPUAPI.makeDemoFrame()
        let b = NativeGPUAPI.makeDemoFrame()
        XCTAssertEqual(a.checksum, b.checksum)
        XCTAssertEqual(a.commands.map(\.kind), [1, 2, 2, 2, 2, 2, 3])
        XCTAssertEqual(a.commands[1].x, 24)
        XCTAssertEqual(a.commands[1].width, 272)
        XCTAssertEqual(a.commands[2].height, 100)
        XCTAssertEqual(a.commands.last?.rgba, 0)
    }
}
