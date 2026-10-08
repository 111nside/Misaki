import Foundation
import XCTest
@testable import Misaki

final class ELFInspectorTests: XCTestCase {
    func testMinimalELF64Header() throws {
        var data = Data(repeating: 0, count: 64)
        data[0] = 0x7f; data[1] = 0x45; data[2] = 0x4c; data[3] = 0x46
        data[4] = 2; data[5] = 1
        data[16] = 2; data[18] = 0x3e
        data[24] = 0x78; data[25] = 0x56
        data[56] = 3
        let header = try ELFInspector.inspect(data)
        XCTAssertEqual(header.machine, 0x3e)
        XCTAssertEqual(header.entryPoint, 0x5678)
        XCTAssertEqual(header.programHeaderCount, 3)
        XCTAssertEqual(header.machineLabel, "x86-64")
    }

    func testRejectsNonELF() {
        XCTAssertThrowsError(try ELFInspector.inspect(Data(repeating: 0, count: 64)))
    }

    func testRejectsTruncatedHeader() {
        XCTAssertThrowsError(try ELFInspector.inspect(Data([0x7f, 0x45])))
    }

    func testRejectsNonLittleEndian() {
        var data = Data(repeating: 0, count: 64)
        data[0] = 0x7f; data[1] = 0x45; data[2] = 0x4c; data[3] = 0x46
        data[4] = 2; data[5] = 2
        XCTAssertThrowsError(try ELFInspector.inspect(data))
    }
}
