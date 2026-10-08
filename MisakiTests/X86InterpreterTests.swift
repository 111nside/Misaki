import XCTest
@testable import Misaki

final class X86InterpreterTests: XCTestCase {
    func testMoveAddHalt() throws {
        var cpu = X86Interpreter()
        cpu.load([0x48, 0xB8, 5, 0, 0, 0, 0, 0, 0, 0, 0x48, 0x05, 3, 0, 0, 0, 0xF4])
        try cpu.run()
        XCTAssertEqual(cpu.rax, 8)
        XCTAssertTrue(cpu.isHalted)
    }
    func testRejectsUnsupportedOpcode() {
        var cpu = X86Interpreter()
        cpu.load([0x0F])
        XCTAssertThrowsError(try cpu.step())
    }
    func testUnmappedMemory() {
        let memory = VirtualMemory()
        XCTAssertThrowsError(try memory.read8(0xDEADBEEF))
    }
    func testStepLimit() {
        var cpu = X86Interpreter()
        cpu.load([0x90, 0x90])
        XCTAssertThrowsError(try cpu.run(maxSteps: 1))
    }
}
