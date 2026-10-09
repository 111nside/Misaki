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

    func testCPlusPlusGuestELFActuallyExecutesAcrossLinkedLibrary() {
        let result = NativeCoreAPI.runCPUExecutionDiagnostic()
        XCTAssertTrue(result.passed)
        XCTAssertEqual(result.status, 0)
        XCTAssertEqual(result.abiVersion, 1)
        XCTAssertEqual(result.loadedSegments, 2)
        XCTAssertEqual(result.imports, 1)
        XCTAssertEqual(result.instructions, 5)
        XCTAssertEqual(result.rax, 42)
        XCTAssertTrue(result.halted)
        XCTAssertTrue(result.stackRestored)
        XCTAssertTrue(result.importReadOnly)
        XCTAssertEqual(result.linkedAddress, 0x3000)
    }

    func testCooperativeGuestServicesFromSwift() {
        let result = NativeCoreAPI.runGuestServicesDiagnostic()
        XCTAssertTrue(result.passed)
        XCTAssertEqual(result.status, 0)
        XCTAssertEqual(result.threads, 2)
        XCTAssertEqual(result.instructions, 30)
        XCTAssertEqual(result.yields, 2)
        XCTAssertEqual(result.serviceCalls, 8)
        XCTAssertEqual(result.writeCalls, 2)
        XCTAssertEqual(result.outputBytes, 4)
        XCTAssertTrue(result.outputMatches)
        XCTAssertTrue(result.threadsHalted)
        XCTAssertTrue(result.stacksRestored)
        XCTAssertEqual(result.firstThreadRAX, 4097)
        XCTAssertEqual(result.secondThreadRAX, 4098)
    }

    func testNativeCoreRejectsInvalidFile() {
        let result = NativeCoreAPI.inspectELF([0, 1, 2, 3])
        XCTAssertEqual(result.code, -2)
        XCTAssertEqual(result.type, 0)
        XCTAssertEqual(result.segments, 0)
    }

    func testNativeDynamicELFLoaderRunsOnExpandedBackend() {
        let result = NativeCoreAPI.runDynamicModuleDiagnostic()
        XCTAssertTrue(result.passed)
        XCTAssertEqual(result.status, 0)
        XCTAssertEqual(result.abiVersion, 1)
        XCTAssertEqual(result.segments, 2)
        XCTAssertEqual(result.imports, 1)
        XCTAssertEqual(result.relativeRelocations, 1)
        XCTAssertEqual(result.instructions, 5)
        XCTAssertEqual(result.rax, 42)
        XCTAssertEqual(result.entry, 0x5000)
        XCTAssertEqual(result.linkedAddress, 0x9000)
        XCTAssertEqual(result.relativeValue, 0x5234)
        XCTAssertTrue(result.halted)
        XCTAssertTrue(result.stackRestored)
        XCTAssertTrue(result.importReadOnly)
    }

    func testExpandedX64BackendFromSwift() {
        let result = NativeCoreAPI.runX64BackendDiagnostic()
        XCTAssertTrue(result.passed)
        XCTAssertEqual(result.status, 0)
        XCTAssertEqual(result.abiVersion, 1)
        XCTAssertEqual(result.backendId, 1)
        XCTAssertEqual(result.rax, 44)
        XCTAssertEqual(result.instructions, 13)
        XCTAssertEqual(result.imports, 1)
        XCTAssertEqual(result.linkedAddress, 0x3000)
        XCTAssertTrue(result.halted)
        XCTAssertTrue(result.stackRestored)
        XCTAssertTrue(result.importReadOnly)
        XCTAssertFalse(result.zeroFlag) // Final ADD leaves RAX=44, so ZF is clear.
    }
}
