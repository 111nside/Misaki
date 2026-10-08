import XCTest
@testable import Misaki

final class NativeCoreBridgeTests: XCTestCase {
    func testNativeCoreDiagnosticFromSwift() {
        let result = NativeCoreAPI.runDiagnostic()
        XCTAssertTrue(result.passed)
        XCTAssertEqual(result.status, 0)
        XCTAssertEqual(result.abiVersion, 1)
        XCTAssertEqual(result.elfType, 3)
        XCTAssertEqual(result.loadSegments, 1)
        XCTAssertEqual(result.registeredModules, 1)
        XCTAssertEqual(result.resolvedImports, 1)
        XCTAssertEqual(result.linkedGuestAddress, 0x6200)
        XCTAssertTrue(result.importReadOnly)
    }

    func testNativeCoreRejectsInvalidFile() {
        let result = NativeCoreAPI.inspectELF([0, 1, 2, 3])
        XCTAssertEqual(result.code, -2)
        XCTAssertEqual(result.type, 0)
        XCTAssertEqual(result.segments, 0)
    }
}
