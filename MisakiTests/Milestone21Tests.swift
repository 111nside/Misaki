import XCTest
@testable import Misaki

final class Milestone21Tests: XCTestCase {
    func testAdvancedGraphicsAndTwoTextures() {
        let result = NativeGraphics21API.makeFrame()
        XCTAssertTrue(result.passed)
        XCTAssertEqual(result.status, 0)
        XCTAssertEqual(result.width, 320)
        XCTAssertEqual(result.height, 180)
        XCTAssertEqual(result.commands, 10)
        XCTAssertEqual(result.rectangles, 2)
        XCTAssertEqual(result.sprites, 2)
        XCTAssertEqual(result.distinctTextures, 2)
        XCTAssertEqual(result.stateChanges, 2)
        XCTAssertEqual(result.scissorChanges, 2)
        XCTAssertEqual(result.guestInstructions, 15)
        XCTAssertEqual(result.guestServiceCalls, 1)
        XCTAssertEqual(result.firstSpriteX, 32)
        XCTAssertEqual(result.secondSpriteX, 208)
        XCTAssertEqual(result.submittedFence, result.completedFence)
        XCTAssertTrue(result.queueDrained)
        XCTAssertTrue(result.stackRestored)
        XCTAssertTrue(result.guestHalted)
        XCTAssertEqual(result.pixels.count, 320 * 180 * 4)
    }

    func testGuestChoosesAllShaderModes() {
        for mode: UInt32 in 0...3 {
            let result = NativeGraphics21API.makeFrame(index: 20, effect: mode)
            XCTAssertTrue(result.passed)
            XCTAssertEqual(result.effectMode, mode)
            XCTAssertEqual(result.firstSpriteX, 72)
            XCTAssertEqual(result.secondSpriteX, 188)
            XCTAssertEqual(result.submittedFence, 21)
        }
    }

    func testFrameBufferStableAndMoves() {
        let start = NativeGraphics21API.makeFrame(index: 0)
        let again = NativeGraphics21API.makeFrame(index: 0)
        let moved = NativeGraphics21API.makeFrame(index: 1)
        XCTAssertEqual(start.checksum, again.checksum)
        XCTAssertNotEqual(start.checksum, moved.checksum)
        XCTAssertEqual(NativeGraphics21API.makeFrame(index: 90).firstSpriteX, 32)
    }

    func testRejectUnsupportedShaderMode() {
        let invalid = NativeGraphics21API.makeFrame(effect: 4)
        XCTAssertFalse(invalid.passed)
        XCTAssertEqual(invalid.status, -2)
        XCTAssertTrue(invalid.pixels.isEmpty)
    }
}
