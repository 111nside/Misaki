import Foundation
import XCTest
@testable import Misaki

final class ELFLoaderTests: XCTestCase {
    func testDemoELFRunsToHalt() throws {
        var cpu = X86Interpreter()
        try cpu.loadELF(DemoELF.make())
        XCTAssertEqual(cpu.rip, 0x1000)
        try cpu.run(maxSteps: 20)
        XCTAssertTrue(cpu.isHalted)
        XCTAssertEqual(cpu.rax, 8)
    }

    func testBSSIsZeroFilled() throws {
        let image = try ELFLoader.load(DemoELF.make())
        XCTAssertEqual(image.loadedSegments, 1)
        XCTAssertEqual(try image.memory.read8(0x1000 + 17), 0)
        XCTAssertThrowsError(try image.memory.read8(0x1000 + 33))
    }

    func testRejectsWrongArchitecture() {
        var data = DemoELF.make()
        data[18] = 0xB7
        XCTAssertThrowsError(try ELFLoader.load(data)) { error in
            XCTAssertEqual(error as? ELFLoadError, .unsupportedArchitecture)
        }
    }

    func testRejectsTruncatedProgramTable() {
        var data = DemoELF.make()
        data[32] = 0xFF // phoff now 255; insufficient bytes for 56 byte entry
        XCTAssertThrowsError(try ELFLoader.load(data)) { error in
            XCTAssertEqual(error as? ELFLoadError, .invalidProgramHeaders)
        }
    }

    func testRejectsInvalidSegmentSize() {
        var data = DemoELF.make()
        // p_memsz=1 while p_filesz=17
        for offset in 104..<112 { data[offset] = 0 }
        data[104] = 1
        XCTAssertThrowsError(try ELFLoader.load(data)) { error in
            XCTAssertEqual(error as? ELFLoadError, .invalidSegment)
        }
    }

    func testRejectsUnmappedEntryPoint() {
        var data = DemoELF.make()
        data[24] = 0
        data[25] = 0
        XCTAssertThrowsError(try ELFLoader.load(data)) { error in
            XCTAssertEqual(error as? ELFLoadError, .entryPointUnmapped)
        }
    }

    func testRejectsTruncatedFile() {
        let cut = DemoELF.make().prefix(70)
        XCTAssertThrowsError(try ELFLoader.load(Data(cut)))
    }
}
