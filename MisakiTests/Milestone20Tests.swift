import XCTest
@testable import Misaki

final class Milestone20Tests: XCTestCase {
    func testGuestQueueSubmitAndFenceFromSwift() {
        let frame = NativeGraphics20API.makeFrame()
        XCTAssertTrue(frame.passed)
        XCTAssertEqual(frame.status, 0)
        XCTAssertEqual(frame.width, 320)
        XCTAssertEqual(frame.height, 180)
        XCTAssertEqual(frame.commands, 7)
        XCTAssertEqual(frame.rectangles, 4)
        XCTAssertEqual(frame.sprites, 1)
        XCTAssertEqual(frame.guestInstructions, 9)
        XCTAssertEqual(frame.guestServiceCalls, 1)
        XCTAssertTrue(frame.guestHalted)
        XCTAssertTrue(frame.stackRestored)
        XCTAssertTrue(frame.queueDrained)
        XCTAssertEqual(frame.submittedFence, 1)
        XCTAssertEqual(frame.completedFence, 1)
        XCTAssertEqual(frame.spriteX, 32)
        XCTAssertEqual(frame.spriteY, 78)
        XCTAssertNotEqual(frame.checksum, 0)
        XCTAssertEqual(frame.pixels.count, 320 * 180 * 4)
    }

    func testGuestQueueAnimationAndDeterminism() {
        let first = NativeGraphics20API.makeFrame(index: 0)
        let second = NativeGraphics20API.makeFrame(index: 1)
        let wrapped = NativeGraphics20API.makeFrame(index: 90)
        XCTAssertTrue(second.passed)
        XCTAssertTrue(wrapped.passed)
        XCTAssertEqual(second.spriteX, 34)
        XCTAssertEqual(second.submittedFence, 2)
        XCTAssertEqual(second.completedFence, 2)
        XCTAssertNotEqual(first.checksum, second.checksum)
        XCTAssertEqual(first.checksum, wrapped.checksum)
        XCTAssertEqual(wrapped.submittedFence, 91)
    }
}
