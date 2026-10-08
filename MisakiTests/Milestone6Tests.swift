import Foundation
import XCTest
@testable import Misaki

final class Milestone6Tests: XCTestCase {
    private func mov(_ reg: UInt8, _ value: UInt64) -> [UInt8] {
        [0x48, 0xB8 + reg] + (0..<8).map { UInt8(truncatingIfNeeded: value >> ($0 * 8)) }
    }

    func testSignedMultiplyTwoRegisterForm() throws {
        var cpu = X86Interpreter()
        cpu.load(mov(0, 6) + mov(3, 7) + [0x48, 0x0F, 0xAF, 0xC3, 0xF4])
        try cpu.run()
        XCTAssertEqual(cpu.rax, 42)
        XCTAssertFalse(cpu.overflowFlag)
    }

    func testSignedMultiplyOverflow() throws {
        var cpu = X86Interpreter()
        cpu.load(mov(0, UInt64.max >> 1) + mov(3, 3) + [0x48, 0x0F, 0xAF, 0xC3, 0xF4])
        try cpu.run()
        XCTAssertTrue(cpu.overflowFlag)
    }

    func testFullWidthUnsignedMultiply() throws {
        var cpu = X86Interpreter()
        cpu.load(mov(0, UInt64.max) + mov(3, 2) + [0x48, 0xF7, 0xE3, 0xF4])
        try cpu.run()
        XCTAssertEqual(cpu.rax, UInt64.max - 1)
        XCTAssertEqual(cpu.rdx, 1)
        XCTAssertTrue(cpu.carryFlag)
    }

    func testUnsignedDivideWithRemainder() throws {
        var cpu = X86Interpreter()
        cpu.load(mov(0, 100) + mov(3, 7) + [0x48, 0xF7, 0xF3, 0xF4])
        try cpu.run()
        XCTAssertEqual(cpu.rax, 14)
        XCTAssertEqual(cpu.rdx, 2)
    }

    func testDivisionByZeroReportsGuestError() {
        var cpu = X86Interpreter()
        cpu.load(mov(0, 100) + [0x48, 0xF7, 0xF3])
        XCTAssertThrowsError(try cpu.run()) { error in
            XCTAssertEqual(error as? EmulatorError, .divideError)
        }
    }

    func testSignedDivideNegativeDividend() throws {
        var cpu = X86Interpreter()
        cpu.load(mov(0, UInt64(bitPattern: -19)) + mov(2, UInt64.max) + mov(3, 4) + [0x48, 0xF7, 0xFB, 0xF4])
        try cpu.run()
        XCTAssertEqual(Int64(bitPattern: cpu.rax), -4)
        XCTAssertEqual(Int64(bitPattern: cpu.rdx), -3)
    }

    func testCarryAndOverflowFlags() throws {
        var cpu = X86Interpreter()
        cpu.load(mov(0, UInt64.max) + [0x48, 0x05, 0x01, 0, 0, 0, 0xF4])
        try cpu.run()
        XCTAssertTrue(cpu.zeroFlag)
        XCTAssertTrue(cpu.carryFlag)
        XCTAssertFalse(cpu.overflowFlag)
        XCTAssertTrue(cpu.parityFlag)
        cpu.load(mov(0, UInt64(Int64.max)) + [0x48, 0x05, 0x01, 0, 0, 0, 0xF4])
        try cpu.run()
        XCTAssertFalse(cpu.carryFlag)
        XCTAssertTrue(cpu.overflowFlag)
        XCTAssertTrue(cpu.signFlag)
    }

    func testBorrowAndSignedLessBranch() throws {
        var cpu = X86Interpreter()
        // mov rax,2; cmp rax,3; JL +1 (skips first HLT); NOP; HLT
        cpu.load(mov(0, 2) + [0x48, 0x3D, 3, 0, 0, 0, 0x7C, 0x02, 0xF4, 0x90, 0xF4])
        try cpu.run()
        XCTAssertTrue(cpu.isHalted)
        XCTAssertEqual(cpu.executedInstructions, 4)
        XCTAssertTrue(cpu.carryFlag)
        XCTAssertTrue(cpu.signFlag)
    }

    func testNearJNEBranch() throws {
        var cpu = X86Interpreter()
        cpu.load(mov(0, 7) + [0x48, 0x3D, 8, 0, 0, 0, 0x0F, 0x85, 2, 0, 0, 0, 0xF4, 0x90, 0xF4])
        try cpu.run()
        XCTAssertEqual(cpu.executedInstructions, 4)
    }

    func testSIBMemoryDisplacement() throws {
        var cpu = X86Interpreter()
        let code = mov(0, 0x1234) + [0x48, 0x89, 0x44, 0x24, 0xF8,
                                  0x48, 0x8B, 0x4C, 0x24, 0xF8, 0xF4]
        cpu.load(code)
        try cpu.run()
        XCTAssertEqual(cpu.rcx, 0x1234)
        XCTAssertEqual(try cpu.memory.read64(cpu.rsp - 8), 0x1234)
    }

    func testRIPRelativeRead() throws {
        var cpu = X86Interpreter()
        // mov rax,[rip+1]; hlt; 8-byte little-endian constant
        cpu.load([0x48, 0x8B, 0x05, 0x01, 0, 0, 0, 0xF4, 0x2A, 0, 0, 0, 0, 0, 0, 0])
        try cpu.run()
        XCTAssertEqual(cpu.rax, 42)
    }

    func testRIPRelativeAddressWithTrailingImmediate() throws {
        var cpu = X86Interpreter()
        // mov qword ptr [rip+1],42; hlt; 8 bytes writable in raw test mapping
        cpu.load([0x48, 0xC7, 0x05, 1, 0, 0, 0, 42, 0, 0, 0, 0xF4] + [UInt8](repeating: 0, count: 8))
        try cpu.run()
        XCTAssertEqual(try cpu.memory.read64(0x1000 + 12), 42)
    }

    func testLEA() throws {
        var cpu = X86Interpreter()
        cpu.load([0x48, 0x8D, 0x05, 0, 0, 0, 0, 0xF4])
        try cpu.run()
        XCTAssertEqual(cpu.rax, 0x1007)
    }

    func testUnalignedSSEMoveAndPackedXor() throws {
        var cpu = X86Interpreter()
        let code: [UInt8] = [0xF3, 0x0F, 0x6F, 0x05, 8, 0, 0, 0,
                             0x66, 0x0F, 0xEF, 0xC0, 0xF4, 0x90, 0x90, 0x90]
        cpu.load(code + Array(1...16).map(UInt8.init))
        try cpu.step()
        XCTAssertEqual(cpu.xmmBytes(0), Array(1...16).map(UInt8.init))
        try cpu.run()
        XCTAssertEqual(cpu.xmmBytes(0), [UInt8](repeating: 0, count: 16))
    }

    func testScalarFloatAddition() throws {
        var cpu = X86Interpreter()
        let code: [UInt8] = [0xF3, 0x0F, 0x10, 0x05, 16, 0, 0, 0,
                             0xF3, 0x0F, 0x58, 0x05, 12, 0, 0, 0,
                             0xF4] + [UInt8](repeating: 0x90, count: 7)
        let floats = [Float(1.5).bitPattern, Float(2.25).bitPattern]
        let raw = floats.flatMap { n in (0..<4).map { UInt8(truncatingIfNeeded: n >> ($0 * 8)) } }
        cpu.load(code + raw)
        try cpu.run()
        let bits = cpu.xmmBytes(0)[0..<4].enumerated().reduce(UInt32(0)) { $0 | (UInt32($1.element) << ($1.offset * 8)) }
        XCTAssertEqual(Float(bitPattern: bits), 3.75)
    }

    func testSSEAlignedLoadRejectsUnalignedPointer() throws {
        var cpu = X86Interpreter()
        cpu.load([0x66, 0x0F, 0x6F, 0x05, 1, 0, 0, 0, 0xF4] + [UInt8](repeating: 0, count: 32))
        XCTAssertThrowsError(try cpu.step()) { error in
            XCTAssertEqual(error as? EmulatorError, .protectionFault(0x1009))
        }
    }

    func testAtomicWriteBytesRejectsCrossingPermissions() throws {
        var mem = VirtualMemory()
        try mem.map([5, 6], at: 0x2000, permissions: [.read, .write])
        try mem.map([7, 8], at: 0x2002, permissions: [.read])
        XCTAssertThrowsError(try mem.writeBytes(0x2000, values: [1, 2, 3, 4]))
        XCTAssertEqual(try mem.read8(0x2000), 5)
        XCTAssertEqual(try mem.read8(0x2001), 6)
    }

    func testDemoPageSizeService() throws {
        var cpu = X86Interpreter()
        cpu.load(mov(0, 0x100) + [0x0F, 0x05, 0xF4])
        try cpu.run()
        XCTAssertEqual(cpu.rax, 4096)
    }

    func testIsolatedCooperativeScheduler() throws {
        var a = X86Interpreter()
        var b = X86Interpreter()
        a.load([0x90, 0xF4])
        b.load([0x90, 0x90, 0xF4])
        var scheduler = GuestThreadScheduler(guests: [a, b])
        try scheduler.run()
        XCTAssertEqual(scheduler.totalInstructions, 5)
        XCTAssertEqual(scheduler.guests[0].executedInstructions, 2)
        XCTAssertEqual(scheduler.guests[1].executedInstructions, 3)
    }

    func testSchedulerStepCap() {
        var a = X86Interpreter()
        a.load([0xEB, 0xFE]) // infinite JMP to itself
        var scheduler = GuestThreadScheduler(guests: [a])
        XCTAssertThrowsError(try scheduler.run(maxTotalSteps: 5)) { error in
            XCTAssertEqual(error as? EmulatorError, .stepLimit)
        }
    }

    func testInAppExtendedCPUFixture() throws {
        var cpu = X86Interpreter()
        cpu.load(mov(0, 6) + mov(3, 7) + [0x48, 0x0F, 0xAF, 0xC3,
                                      0x48, 0x3D, 42, 0, 0, 0,
                                      0x75, 0x01, 0xF4, 0x90, 0xF4])
        try cpu.run(maxSteps: 20)
        XCTAssertTrue(cpu.isHalted)
        XCTAssertEqual(cpu.rax, 42)
        XCTAssertTrue(cpu.zeroFlag)
        XCTAssertEqual(cpu.rip, 0x1021)
    }

    func testOriginalStackDemoStillWorks() throws {
        var cpu = X86Interpreter()
        try cpu.loadELF(DemoStackELF.make())
        let initialSP = cpu.rsp
        try cpu.run(maxSteps: 40)
        XCTAssertEqual(cpu.rax, 8)
        XCTAssertEqual(cpu.rsp, initialSP)
        XCTAssertTrue(cpu.isHalted)
    }

    func testOriginalUserspaceDemoStillWorks() throws {
        var cpu = X86Interpreter()
        try cpu.loadELF(DemoKernelELF.make())
        try cpu.run(maxSteps: 50)
        XCTAssertEqual(cpu.consoleOutput, "Misaki!\n")
        XCTAssertEqual(cpu.exitCode, 0)
    }
}
