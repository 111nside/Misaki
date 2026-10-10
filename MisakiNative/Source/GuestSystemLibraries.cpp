#include "../Include/GuestSystemLibraries.hpp"

#include <algorithm>
#include <limits>
#include <utility>

namespace misaki {
namespace {
constexpr std::uint64_t moduleBias = 0x4000;
constexpr std::uint64_t libraryAddress = 0x9000;
constexpr std::uint64_t dataAddress = 0xA000;
constexpr std::uint64_t bufferAddress = 0xA100;
constexpr std::size_t dataSize = 0x200;
constexpr std::uint32_t demoPID = 1001;
constexpr const char *guestPath = "/system/message.txt";

void movImm64(std::vector<std::uint8_t> &out, std::uint8_t index,
              std::uint64_t value) {
    // The selected argument registers are RAX/RDX/RBX/RSI/RDI (0,2,3,6,7).
    out.push_back(0x48);
    out.push_back(static_cast<std::uint8_t>(0xB8 + index));
    for (unsigned i = 0; i < 8; ++i)
        out.push_back(static_cast<std::uint8_t>(value >> (8 * i)));
}
void syscall(std::vector<std::uint8_t> &out) {
    out.push_back(0x0F);
    out.push_back(0x05);
}
void copyFDToRDI(std::vector<std::uint8_t> &out) {
    out.insert(out.end(), {0x48, 0x89, 0xDF}); // MOV RDI,RBX
}
}

std::vector<std::uint8_t> makeGuestSystemLibraryCode() {
    std::vector<std::uint8_t> code;
    // fd = open("/system/message.txt", readonly)
    movImm64(code, 0, process_service::open);
    movImm64(code, 7, dataAddress);
    movImm64(code, 6, 0);
    syscall(code);
    code.insert(code.end(), {0x48, 0x89, 0xC3}); // MOV RBX,RAX

    // n = read(fd, guestBuffer, 5)
    movImm64(code, 0, process_service::read);
    copyFDToRDI(code);
    movImm64(code, 6, bufferAddress);
    movImm64(code, 2, 5);
    syscall(code);

    // write(guestBuffer, 5) through the toy guest service interface
    movImm64(code, 0, guest_service::write);
    movImm64(code, 7, bufferAddress);
    movImm64(code, 6, 5);
    syscall(code);

    // close(fd)
    movImm64(code, 0, process_service::close);
    copyFDToRDI(code);
    syscall(code);

    // Consult the simulated process identifier and page size, demonstrating
    // they coexist with VFS services in the same per-process dispatcher.
    movImm64(code, 0, process_service::processID);
    syscall(code);
    movImm64(code, 0, guest_service::pageSize);
    syscall(code);
    // ADD RAX, -4054 : 4096 -> 42. This uses a real guest arithmetic opcode.
    code.insert(code.end(), {0x48, 0x05, 0x2A, 0xF0, 0xFF, 0xFF});
    code.push_back(0xC3); // RET into dynamically loaded program
    return code;
}

std::optional<GuestSystemLibraryReport> runGuestSystemLibraryDiagnostic() {
    GuestVirtualFileSystem files;
    const std::vector<std::uint8_t> greeting{'H', 'e', 'l', 'l', 'o'};
    if (!files.addFile(guestPath, greeting)) return std::nullopt;

    GuestMemory memory;
    auto library = makeGuestSystemLibraryCode();
    if (library.empty() || library.size() > 512 ||
        !memory.map(libraryAddress, library.size(), permission::read | permission::write) ||
        !memory.writeBytes(libraryAddress, library.data(), library.size()) ||
        !memory.protect(libraryAddress, library.size(), permission::read | permission::execute))
        return std::nullopt;

    const std::string path = guestPath;
    std::vector<std::uint8_t> data(dataSize, 0);
    std::copy(path.begin(), path.end(), data.begin());
    if (!memory.map(dataAddress, data.size(), permission::read | permission::write) ||
        !memory.writeBytes(dataAddress, data.data(), data.size()))
        return std::nullopt;

    // The dynamic module is built by the previous milestone's genuine
    // ELF64 fixture builder, complete with DT_NEEDED, DT_SYMTAB and RELA.
    auto elf = makeDynamicCPUFixture();
    ModuleRegistry symbols;
    if (!symbols.registerModule({"libMisakiDynamic", {{"increment2", libraryAddress}}}))
        return std::nullopt;
    auto linked = loadAndLinkDynamicELF(elf.data(), elf.size(), moduleBias, symbols, memory);
    if (!linked || linked->importedSymbols != 1 || linked->relativeRelocations != 1 ||
        linked->neededLibraries != 1) return std::nullopt;

    const auto import = memory.read64(0x5100);
    if (!import || *import != libraryAddress) return std::nullopt;
    // Fixture-only RELRO: in real ET_DYN modules, data segments may still
    // require writable globals. Do not apply this policy to arbitrary files.
    if (!memory.protect(0x5100, 0x240, permission::read)) return std::nullopt;
    const bool importReadOnly = !memory.write64(0x5100, 0);
    const bool libraryReadOnly = !memory.writeBytes(libraryAddress, library.data(), 1);

    GuestProcessServices services(files, demoPID);
    PortableX64Backend cpu(std::move(memory), linked->entry, &services, demoPID);
    GuestSystemLibraryReport report;
    report.execution = cpu.run(128);
    report.linked = *linked;
    report.importAddress = *import;
    report.processID = services.pid();
    report.opens = services.opens();
    report.reads = services.reads();
    report.closes = services.closes();
    report.writes = services.writes();
    report.yields = services.yields();
    report.output = services.output();
    report.importReadOnly = importReadOnly;
    report.libraryReadOnly = libraryReadOnly;
    return report;
}

} // namespace misaki
