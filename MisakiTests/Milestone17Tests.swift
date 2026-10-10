import XCTest
@testable import Misaki

final class Milestone17Tests: XCTestCase {
    func testNativeModuleLifecycleEndToEnd() {
        let snapshot = NativeCoreAPI.runModuleLifecycleDiagnostic()
        XCTAssertTrue(snapshot.passed)
        XCTAssertEqual(snapshot.status, 0)
        XCTAssertEqual(snapshot.abiVersion, 1)
        XCTAssertEqual(snapshot.modules, 3)
        XCTAssertEqual(snapshot.imports, 1)
        XCTAssertEqual(snapshot.instructions, 5)
        XCTAssertEqual(snapshot.initializations, 3)
        XCTAssertEqual(snapshot.finalizations, 3)
        XCTAssertEqual(snapshot.rax, 42)
        XCTAssertTrue(snapshot.stackRestored)
        XCTAssertTrue(snapshot.importReadOnly)
        XCTAssertTrue(snapshot.unloaded)
        XCTAssertTrue(snapshot.dependencyOrderValid)
    }
}
