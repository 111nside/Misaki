import XCTest
@testable import Misaki

final class Milestone31to35Tests: XCTestCase {
    func testFiveIntegratedSubsystemsOnSimulator() {
        let frame = NativeIntegratedRuntimeAPI.run()
        XCTAssertEqual(frame.status, 0)
        XCTAssertTrue(frame.passed)
        XCTAssertEqual(frame.stages, [true, true, true, true, true])
        XCTAssertEqual(frame.processCount, 2)
        XCTAssertEqual(frame.fileReads, 1)
        XCTAssertEqual(frame.gpuCalls, 1)
        XCTAssertEqual(frame.pm4Packets, 11)
        XCTAssertEqual(frame.shaderOperations, 11)
        XCTAssertEqual(frame.loadedModules, 2)
        XCTAssertEqual(frame.resolvedImports, 1)
        XCTAssertEqual(frame.elfRAX, 42)
        XCTAssertEqual(frame.guestPID, 1001)
        XCTAssertEqual(frame.outputRecord7, 31)
    }

    func testGuestFramebufferIsRealAndDeterministic() {
        let a = NativeIntegratedRuntimeAPI.run()
        let b = NativeIntegratedRuntimeAPI.run()
        XCTAssertTrue(a.passed)
        XCTAssertTrue(b.passed)
        XCTAssertEqual(a.submittedFence, 1)
        XCTAssertEqual(a.completedFence, 1)
        XCTAssertEqual(a.width, 320)
        XCTAssertEqual(a.height, 180)
        XCTAssertEqual(a.sprites, 2)
        XCTAssertEqual(a.pixels.count, 320 * 180 * 4)
        XCTAssertEqual(a.checksum, b.checksum)
        XCTAssertEqual(a.pixels, b.pixels)
    }
}
