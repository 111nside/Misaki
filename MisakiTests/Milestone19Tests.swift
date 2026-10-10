import XCTest
@testable import Misaki

final class Milestone19Tests: XCTestCase {
    func testGuestCPUToTextureToFramebuffer() {
        let frame = NativeGraphics19API.makeFrame()
        XCTAssertTrue(frame.passed)
        XCTAssertEqual(frame.status, 0)
        XCTAssertEqual(frame.width, 320)
        XCTAssertEqual(frame.height, 180)
        XCTAssertEqual(frame.commands, 7)
        XCTAssertEqual(frame.rectangles, 4)
        XCTAssertEqual(frame.sprites, 1)
        XCTAssertEqual(frame.guestInstructions, 6)
        XCTAssertEqual(frame.guestServiceCalls, 1)
        XCTAssertTrue(frame.guestHalted)
        XCTAssertTrue(frame.stackRestored)
        XCTAssertEqual(frame.spriteX, 32)
        XCTAssertEqual(frame.spriteY, 78)
        XCTAssertNotEqual(frame.checksum, 0)
        XCTAssertEqual(frame.pixels.count, 320 * 180 * 4)
        XCTAssertEqual(Array(frame.pixels.prefix(4)), [12, 20, 38, 255])
    }

    func testGuestControlsSpriteAnimation() {
        let first = NativeGraphics19API.makeFrame(index: 0)
        let next = NativeGraphics19API.makeFrame(index: 1)
        let wrapped = NativeGraphics19API.makeFrame(index: 90)
        XCTAssertTrue(first.passed)
        XCTAssertTrue(next.passed)
        XCTAssertTrue(wrapped.passed)
        XCTAssertEqual(next.spriteX, 34)
        XCTAssertEqual(wrapped.spriteX, 32)
        XCTAssertNotEqual(first.checksum, next.checksum)
        XCTAssertEqual(first.checksum, wrapped.checksum)
    }
}
