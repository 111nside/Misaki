import Foundation
import XCTest
@testable import Misaki

final class Milestone7Tests: XCTestCase {
    private func put(_ value: UInt64, into bytes: inout [UInt8], at offset: Int, width: Int) {
        for index in 0..<width {
            bytes[offset + index] = UInt8(truncatingIfNeeded: value >> (index * 8))
        }
    }

    private func relaFixture(kind: UInt32 = 8, relocations: Int = 1) -> Data {
        // A self-authored ELF64 file with one SHT_RELA section, not PS4 firmware.
        var bytes = [UInt8](repeating: 0, count: 0x100 + relocations * 24)
        bytes[0] = 0x7f; bytes[1] = 0x45; bytes[2] = 0x4c; bytes[3] = 0x46
        bytes[4] = 2; bytes[5] = 1; bytes[6] = 1
        put(2, into: &bytes, at: 16, width: 2) // ET_EXEC
        put(0x3e, into: &bytes, at: 18, width: 2)
        put(0x80, into: &bytes, at: 40, width: 8) // e_shoff
        put(64, into: &bytes, at: 58, width: 2)  // e_shentsize
        put(1, into: &bytes, at: 60, width: 2)   // e_shnum
        put(4, into: &bytes, at: 0x80 + 4, width: 4) // SHT_RELA
        put(0x100, into: &bytes, at: 0x80 + 24, width: 8) // sh_offset
        put(UInt64(relocations * 24), into: &bytes, at: 0x80 + 32, width: 8)
        put(24, into: &bytes, at: 0x80 + 56, width: 8)
        for i in 0..<relocations {
            let entry = 0x100 + i * 24
            put(0x6000 + UInt64(i * 8), into: &bytes, at: entry, width: 8)
            put(UInt64(kind), into: &bytes, at: entry + 8, width: 8)
            put(0x30 + UInt64(i), into: &bytes, at: entry + 16, width: 8)
        }
        return Data(bytes)
    }

    func testLinkedExecutableHasSeparateCodeAndGOTSegments() throws {
        var image = try ELFLoader.load(GuestLinkedELF.make())
        XCTAssertEqual(image.loadedSegments, 2)
        XCTAssertEqual(image.entryPoint, 0x1000)
        XCTAssertEqual(try image.memory.fetch8(0x1000), 0x48)
        XCTAssertEqual(try image.memory.read64(0x1100), 0)
        XCTAssertThrowsError(try image.memory.write8(0x1000, value: 0))
        XCTAssertThrowsError(try image.memory.fetch8(0x1100))
    }

    func testImportedLibraryCallActuallyExecutes() throws {
        let result = try GuestLibraryDemo.run()
        XCTAssertTrue(result.passed)
        XCTAssertEqual(result.value, 42)
        XCTAssertEqual(result.instructions, 5) // CALL, MOV, ADD, RET, HLT
        XCTAssertEqual(result.linkedImports, 1)
        XCTAssertTrue(result.stackRestored)
    }

    func testLinkedPointerIsReadOnlyAndLibraryIsExecutable() throws {
        var fixture = try GuestLibraryDemo.preparedCPU()
        XCTAssertEqual(try fixture.cpu.memory.read64(0x1100), 0x3000)
        XCTAssertEqual(try fixture.cpu.memory.fetch8(0x3000), 0x48)
        XCTAssertThrowsError(try fixture.cpu.memory.write64(0x1100, value: 0)) { error in
            XCTAssertEqual(error as? EmulatorError, .protectionFault(0x1100))
        }
        XCTAssertThrowsError(try fixture.cpu.memory.write8(0x3000, value: 0x90)) { error in
            XCTAssertEqual(error as? EmulatorError, .protectionFault(0x3000))
        }
    }

    func testDuplicateLibrariesAreRejected() throws {
        var linker = GuestDynamicLinker()
        let module = GuestLibrary(name: "libA", exports: ["method": 0x1234])
        try linker.register(module)
        XCTAssertThrowsError(try linker.register(module)) { error in
            XCTAssertEqual(error as? GuestLinkError, .duplicateLibrary("libA"))
        }
    }

    func testUnresolvedSymbolDoesNotPatchEarlierImport() throws {
        var mem = VirtualMemory()
        try mem.mapZeroed(at: 0x4000, size: 16, permissions: [.read, .write])
        var linker = GuestDynamicLinker()
        try linker.register(GuestLibrary(name: "libA", exports: ["known": 0x1234]))
        let imports = [
            GuestImport(library: "libA", symbol: "known", slotAddress: 0x4000),
            GuestImport(library: "libA", symbol: "missing", slotAddress: 0x4008)
        ]
        XCTAssertThrowsError(try linker.link(imports, into: &mem)) { error in
            XCTAssertEqual(error as? GuestLinkError, .unresolvedSymbol("libA", "missing"))
        }
        XCTAssertEqual(try mem.read64(0x4000), 0)
        XCTAssertEqual(try mem.read64(0x4008), 0)
    }

    func testLinkRollsBackOnUnwritableImport() throws {
        var mem = VirtualMemory()
        try mem.mapZeroed(at: 0x4000, size: 8, permissions: [.read, .write])
        try mem.mapZeroed(at: 0x5000, size: 8, permissions: [.read])
        var linker = GuestDynamicLinker()
        try linker.register(GuestLibrary(name: "libA", exports: ["fn": 0x3000]))
        XCTAssertThrowsError(try linker.link([
            GuestImport(library: "libA", symbol: "fn", slotAddress: 0x4000),
            GuestImport(library: "libA", symbol: "fn", slotAddress: 0x5000)
        ], into: &mem))
        XCTAssertEqual(try mem.read64(0x4000), 0)
    }

    func testWholeRegionProtectAndUnmap() throws {
        var mem = VirtualMemory()
        try mem.mapZeroed(at: 0x5000, size: 16, permissions: [.read, .write])
        try mem.write8(0x5000, value: 42)
        try mem.protect(at: 0x5000, size: 16, permissions: [.read, .execute])
        XCTAssertEqual(try mem.fetch8(0x5000), 42)
        XCTAssertThrowsError(try mem.write8(0x5000, value: 11)) { error in
            XCTAssertEqual(error as? EmulatorError, .protectionFault(0x5000))
        }
        XCTAssertThrowsError(try mem.protect(at: 0x5000, size: 8, permissions: [.read]))
        try mem.unmap(at: 0x5000, size: 16)
        XCTAssertThrowsError(try mem.read8(0x5000)) { error in
            XCTAssertEqual(error as? EmulatorError, .unmappedAddress(0x5000))
        }
        try mem.mapZeroed(at: 0x5000, size: 16, permissions: [.read])
        XCTAssertEqual(try mem.read8(0x5000), 0)
    }

    func testRelativeRelocationParsesAndPatches() throws {
        let elf = relaFixture()
        let items = try GuestRELARelocator.parse(elf)
        XCTAssertEqual(items, [GuestRelativeRelocation(offset: 0x6000, addend: 0x30)])
        var mem = VirtualMemory()
        try mem.mapZeroed(at: 0x8000, size: 8, permissions: [.read, .write])
        let count = try GuestRELARelocator.apply(elf, loadBias: 0x2000, memory: &mem)
        XCTAssertEqual(count, 1)
        XCTAssertEqual(try mem.read64(0x8000), 0x2030)
    }

    func testUnsupportedRelocationIsRejected() {
        let elf = relaFixture(kind: 7) // R_X86_64_JUMP_SLOT needs symbol resolution
        XCTAssertThrowsError(try GuestRELARelocator.parse(elf)) { error in
            XCTAssertEqual(error as? GuestRelocationError, .unsupportedRelocation(7))
        }
    }

    func testMalformedRelaSectionIsRejected() {
        var bytes = [UInt8](relaFixture())
        put(0xFFFF, into: &bytes, at: 0x80 + 32, width: 8) // invalid sh_size
        XCTAssertThrowsError(try GuestRELARelocator.parse(Data(bytes))) { error in
            XCTAssertEqual(error as? GuestRelocationError, .invalidRelocationTable)
        }
    }

    func testRelocationsRollBackOnUnmappedSecondSlot() throws {
        let elf = relaFixture(relocations: 2)
        var mem = VirtualMemory()
        try mem.mapZeroed(at: 0x8000, size: 8, permissions: [.read, .write])
        XCTAssertThrowsError(try GuestRELARelocator.apply(elf, loadBias: 0x2000, memory: &mem))
        XCTAssertEqual(try mem.read64(0x8000), 0)
    }

    func testNoSectionTableHasNoRelocations() throws {
        XCTAssertTrue(try GuestRELARelocator.parse(DemoELF.make()).isEmpty)
    }
}
