import Foundation
import XCTest
@testable import Misaki

final class DemoKernelTests: XCTestCase {
    func testDemoSystemCallELF() throws {
        var cpu = X86Interpreter()
        try cpu.loadELF(DemoKernelELF.make())
        try cpu.run(maxSteps: 30)
        XCTAssertTrue(cpu.isHalted)
        XCTAssertEqual(cpu.exitCode, 0)
        XCTAssertEqual(cpu.consoleOutput, DemoKernelELF.expectedOutput)
        XCTAssertEqual(cpu.executedInstructions, 7)
    }

    func testMovAcrossRegistersAndXor() throws {
        var cpu = X86Interpreter()
        cpu.load([
            0x48, 0xB8, 9, 0, 0, 0, 0, 0, 0, 0, // mov rax, 9
            0x48, 0x89, 0xC7,                   // mov rdi, rax
            0x48, 0x8B, 0xF7,                   // mov rsi, rdi
            0x48, 0x31, 0xFF,                   // xor rdi, rdi
            0xF4
        ])
        try cpu.run(maxSteps: 10)
        XCTAssertEqual(cpu.rax, 9)
        XCTAssertEqual(cpu.rsi, 9)
        XCTAssertEqual(cpu.rdi, 0)
        XCTAssertTrue(cpu.zeroFlag)
    }

    func testCompareAndConditionalBranch() throws {
        var cpu = X86Interpreter()
        cpu.load([
            0x48, 0xB8, 4, 0, 0, 0, 0, 0, 0, 0,
            0x48, 0x3D, 4, 0, 0, 0,  // cmp rax,4
            0x74, 0x06,              // je over add rax,10
            0x48, 0x05, 10, 0, 0, 0,
            0xF4
        ])
        try cpu.run(maxSteps: 10)
        XCTAssertEqual(cpu.rax, 4)
        XCTAssertTrue(cpu.zeroFlag)
    }

    func testUnknownSystemCallFailsClearly() {
        var cpu = X86Interpreter()
        cpu.load([0x48, 0xB8, 99, 0, 0, 0, 0, 0, 0, 0, 0x0F, 0x05])
        XCTAssertThrowsError(try cpu.run(maxSteps: 5)) { error in
            XCTAssertEqual(error as? EmulatorError, .unsupportedSyscall(99))
        }
    }

    func testUnmappedOutputFailsClearly() {
        var cpu = X86Interpreter()
        cpu.load([
            0x48, 0xB8, 1, 0, 0, 0, 0, 0, 0, 0,
            0x48, 0xBF, 0, 0x20, 0, 0, 0, 0, 0, 0,
            0x48, 0xBE, 1, 0, 0, 0, 0, 0, 0, 0,
            0x0F, 0x05
        ])
        XCTAssertThrowsError(try cpu.run(maxSteps: 6)) { error in
            XCTAssertEqual(error as? EmulatorError, .unmappedAddress(0x2000))
        }
    }

    func testReloadResetsDiagnosticState() throws {
        var cpu = X86Interpreter()
        try cpu.loadELF(DemoKernelELF.make())
        try cpu.run(maxSteps: 30)
        cpu.load([0xF4])
        XCTAssertEqual(cpu.consoleOutput, "")
        XCTAssertNil(cpu.exitCode)
        XCTAssertEqual(cpu.executedInstructions, 0)
    }
}
