import Foundation
import XCTest
@testable import Misaki

final class Milestone8Tests: XCTestCase {
    private func edit(_ data: Data, _ position: Int, _ value: UInt64, _ width: Int) -> Data {
        var bytes = [UInt8](data)
        for index in 0..<width {
            bytes[position + index] = UInt8(truncatingIfNeeded: value >> (index * 8))
        }
        return Data(bytes)
    }

    func testETDYNGOTCallExecutesRealGuestFunction() throws {
        let result = try DemoDynamicRunner.run()
        XCTAssertTrue(result.passed)
        XCTAssertEqual(result.output, 42)
        XCTAssertEqual(result.instructions, 5)
        XCTAssertEqual(result.relocations, 2)
        XCTAssertTrue(result.stackRestored)
    }

    func testRelocationTargetsAndOriginalPermissions() throws {
        let module = try GuestDynamicELFLoader.load(DemoDynamicELF.make(),
                                                     loadBias: 0x4000,
                                                     libraries: DemoDynamicELF.libraries())
        XCTAssertEqual(module.entryPoint, 0x5000)
        XCTAssertEqual(try module.memory.read64(0x6000), DemoDynamicELF.functionAddress)
        XCTAssertEqual(try module.memory.read64(0x6008), 0x5000)
        XCTAssertEqual(try module.memory.fetch8(0x5000), 0x48)
        XCTAssertThrowsError(try module.memory.fetch8(0x6000))
        var memory = module.memory
        XCTAssertThrowsError(try memory.write8(0x5000, value: 0x90))
    }

    func testDTSymbolTableExportsDefinedSymbol() throws {
        let module = try GuestDynamicELFLoader.load(DemoDynamicELF.make(),
                                                     loadBias: 0x4000,
                                                     libraries: DemoDynamicELF.libraries())
        XCTAssertEqual(module.exports[DemoDynamicELF.localSymbol], 0x5080)
        XCTAssertEqual(try module.memory.fetch8(0x5080), 0x48)
        XCTAssertNil(module.exports[DemoDynamicELF.importSymbol])
    }

    func testNeededLibraryNamesAndImportList() throws {
        let module = try GuestDynamicELFLoader.load(DemoDynamicELF.make(),
                                                     loadBias: 0x4000,
                                                     libraries: DemoDynamicELF.libraries())
        XCTAssertEqual(module.neededLibraries, [DemoDynamicELF.name])
        XCTAssertEqual(module.boundImports, [DemoDynamicELF.importSymbol])
    }

    func testDifferentLoadBiasRelocatesModule() throws {
        let module = try GuestDynamicELFLoader.load(DemoDynamicELF.make(),
                                                     loadBias: 0x50_000,
                                                     libraries: DemoDynamicELF.libraries())
        XCTAssertEqual(module.entryPoint, 0x51_000)
        XCTAssertEqual(try module.memory.read64(0x52_008), 0x51_000)
        XCTAssertEqual(module.exports[DemoDynamicELF.localSymbol], 0x51_080)
    }

    func testETEXECIsNotAcceptedAsDynamicModule() throws {
        XCTAssertThrowsError(try GuestDynamicELFLoader.load(DemoELF.make(), loadBias: 0)) { error in
            XCTAssertEqual(error as? GuestDynamicELFError, .unsupportedImage)
        }
    }

    func testMissingLibraryDoesNotBindUnresolvedImport() throws {
        XCTAssertThrowsError(try GuestDynamicELFLoader.load(DemoDynamicELF.make(), loadBias: 0x4000)) { error in
            XCTAssertEqual(error as? GuestDynamicELFError, .unresolvedSymbol(DemoDynamicELF.importSymbol))
        }
    }

    func testUnsupportedRelocationIsRejected() throws {
        let elf = edit(DemoDynamicELF.make(), 0x4e0, (UInt64(1) << 32) | 12, 8)
        XCTAssertThrowsError(try GuestDynamicELFLoader.load(elf, loadBias: 0x4000,
                                                            libraries: DemoDynamicELF.libraries())) { error in
            XCTAssertEqual(error as? GuestDynamicELFError, .unsupportedRelocation(12))
        }
    }

    func testOutOfBoundsSymbolIndexIsRejected() throws {
        let elf = edit(DemoDynamicELF.make(), 0x4e0, (UInt64(10) << 32) | 7, 8)
        XCTAssertThrowsError(try GuestDynamicELFLoader.load(elf, loadBias: 0x4000,
                                                            libraries: DemoDynamicELF.libraries())) { error in
            XCTAssertEqual(error as? GuestDynamicELFError, .malformedSymbolTable)
        }
    }

    func testTruncatedDynamicTableIsRejected() throws {
        // Drop the final DT_NULL entry from PT_DYNAMIC's reported size.
        let elf = edit(DemoDynamicELF.make(), 64 + 112 + 32, 12 * 16, 8)
        XCTAssertThrowsError(try GuestDynamicELFLoader.load(elf, loadBias: 0x4000,
                                                            libraries: DemoDynamicELF.libraries())) { error in
            XCTAssertEqual(error as? GuestDynamicELFError, .malformedDynamicTable)
        }
    }

    func testInvalidDynamicSymbolEntrySizeIsRejected() throws {
        let elf = edit(DemoDynamicELF.make(), 0x310 + 4 * 16 + 8, 16, 8) // DT_SYMENT
        XCTAssertThrowsError(try GuestDynamicELFLoader.load(elf, loadBias: 0x4000,
                                                            libraries: DemoDynamicELF.libraries())) { error in
            XCTAssertEqual(error as? GuestDynamicELFError, .malformedDynamicTable)
        }
    }

    func testBadRelocationLengthIsRejected() throws {
        let elf = edit(DemoDynamicELF.make(), 0x310 + 7 * 16 + 8, 25, 8) // DT_RELASZ
        XCTAssertThrowsError(try GuestDynamicELFLoader.load(elf, loadBias: 0x4000,
                                                            libraries: DemoDynamicELF.libraries())) { error in
            XCTAssertEqual(error as? GuestDynamicELFError, .malformedRelocationTable)
        }
    }

    func testDuplicateRelocationSlotsAreRejected() throws {
        let elf = edit(DemoDynamicELF.make(), 0x4c0, 0x2000, 8) // same slot as JUMP_SLOT
        XCTAssertThrowsError(try GuestDynamicELFLoader.load(elf, loadBias: 0x4000,
                                                            libraries: DemoDynamicELF.libraries())) { error in
            XCTAssertEqual(error as? GuestDynamicELFError, .malformedRelocationTable)
        }
    }

    func testHugeDynamicHashSymbolCountIsRejected() throws {
        let elf = edit(DemoDynamicELF.make(), 0x494, 4096, 4)
        XCTAssertThrowsError(try GuestDynamicELFLoader.load(elf, loadBias: 0x4000,
                                                            libraries: DemoDynamicELF.libraries())) { error in
            XCTAssertEqual(error as? GuestDynamicELFError, .malformedSymbolTable)
        }
    }

    func testExistingLinkedLibraryDemoStillExecutes() throws {
        let result = try GuestLibraryDemo.run()
        XCTAssertTrue(result.passed)
    }

    func testExistingSystemCallDemoStillExecutes() throws {
        var cpu = X86Interpreter()
        try cpu.loadELF(DemoKernelELF.make())
        try cpu.run(maxSteps: 40)
        XCTAssertEqual(cpu.consoleOutput, "Misaki!\n")
        XCTAssertEqual(cpu.exitCode, 0)
    }
}
