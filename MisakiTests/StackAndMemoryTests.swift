import Foundation
import XCTest
@testable import Misaki

final class StackAndMemoryTests: XCTestCase {
    func testCallAndReturnThroughLoadedELF() throws {
        var cpu = X86Interpreter()
        try cpu.loadELF(DemoStackELF.make())
        let initialStack = cpu.rsp
        try cpu.run(maxSteps: 20)
        XCTAssertTrue(cpu.isHalted)
        XCTAssertEqual(cpu.rax, 8)
        XCTAssertEqual(cpu.rcx, 5)
        XCTAssertEqual(cpu.rsp, initialStack)
    }

    func testPushPopSignExtendedImmediate() throws {
        var cpu = X86Interpreter()
        cpu.load([0x6A, 0xFF, 0x58, 0xF4]) // PUSH -1; POP RAX; HLT
        let stack = cpu.rsp
        try cpu.run()
        XCTAssertEqual(cpu.rax, UInt64.max)
        XCTAssertEqual(cpu.rsp, stack)
    }

    func testCallStackUnderflowIsTrapped() throws {
        var cpu = X86Interpreter()
        cpu.load([0xC3]) // RET without any CALL
        XCTAssertThrowsError(try cpu.step()) { error in
            XCTAssertEqual(error as? EmulatorError, .unmappedAddress(cpu.rsp))
        }
    }

    func testRXCodeRejectsWrites() throws {
        var cpu = X86Interpreter()
        try cpu.loadELF(DemoELF.make())
        XCTAssertEqual(try cpu.memory.fetch8(0x1000), 0x48)
        XCTAssertThrowsError(try cpu.memory.write8(0x1000, value: 0x90)) { error in
            XCTAssertEqual(error as? EmulatorError, .protectionFault(0x1000))
        }
    }

    func testStackRejectsExecution() throws {
        var cpu = X86Interpreter()
        cpu.load([0xF4])
        XCTAssertThrowsError(try cpu.memory.fetch8(cpu.rsp - 1)) { error in
            XCTAssertEqual(error as? EmulatorError, .protectionFault(cpu.rsp - 1))
        }
    }

    func testWrite64IsAtomicOnProtectionFailure() throws {
        var memory = VirtualMemory()
        try memory.map([0, 0, 0, 0], at: 0x4000, permissions: [.read, .write])
        try memory.map([0, 0, 0, 0], at: 0x4004, permissions: [.read])
        XCTAssertThrowsError(try memory.write64(0x4000, value: 123))
        XCTAssertEqual(try memory.read8(0x4000), 0)
    }

    func testOverlappingMappingsAreRejected() throws {
        var memory = VirtualMemory()
        try memory.map([1, 2], at: 0x1000, permissions: [.read])
        XCTAssertThrowsError(try memory.map([3], at: 0x1001, permissions: [.read])) { error in
            XCTAssertEqual(error as? EmulatorError, .overlappingMapping)
        }
    }

    func testReadWriteOnlyStackArea() throws {
        var memory = VirtualMemory()
        try memory.map([UInt8](repeating: 0, count: 8), at: 0x3000, permissions: [.read, .write])
        try memory.write64(0x3000, value: 0x1122334455667788)
        XCTAssertEqual(try memory.read64(0x3000), 0x1122334455667788)
    }
}
