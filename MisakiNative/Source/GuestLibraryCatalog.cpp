#include "../Include/GuestLibraryCatalog.hpp"

#include <algorithm>
#include <limits>
#include <utility>

namespace misaki {
namespace {
bool validName(const std::string &name) {
    if (name.empty() || name.size() > 96) return false;
    // No host paths, escaping, spaces, or embedded control characters.
    for (unsigned char c : name) {
        if (!((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
              (c >= '0' && c <= '9') || c == '_' || c == '-' || c == '.'))
            return false;
    }
    return name != "." && name != ".." && name.find("..") == std::string::npos;
}
void put(std::vector<std::uint8_t> &bytes, std::size_t offset,
         std::uint64_t value, unsigned size) {
    for (unsigned i = 0; i < size; ++i)
        bytes[offset + i] = static_cast<std::uint8_t>(value >> (8 * i));
}
}

bool GuestLibraryCatalog::registerLibrary(CatalogLibrary library) {
    if (!validName(library.name) || library.version.major == 0 ||
        library.exports.empty() || library.exports.size() > 128 ||
        libraries_.size() >= 64) return false;
    for (const auto &entry : library.exports) {
        if (!validName(entry.first) || entry.second == 0) return false;
    }
    for (const auto &entry : libraries_) {
        if (entry.name == library.name && entry.version == library.version) return false;
    }
    libraries_.push_back(std::move(library));
    return true;
}

std::size_t GuestLibraryCatalog::libraryCount() const {
    std::size_t result = 0;
    for (std::size_t i = 0; i < libraries_.size(); ++i) {
        bool first = true;
        for (std::size_t j = 0; j < i; ++j)
            if (libraries_[j].name == libraries_[i].name) first = false;
        if (first) ++result;
    }
    return result;
}

const CatalogLibrary *GuestLibraryCatalog::select(
    const std::string &name, std::optional<std::uint16_t> major,
    std::uint16_t minimumMinor) const {
    const CatalogLibrary *choice = nullptr;
    for (const auto &library : libraries_) {
        if (library.name != name) continue;
        if (major && library.version.major != *major) continue;
        if (major && library.version.minor < minimumMinor) continue;
        if (!choice || choice->version < library.version) choice = &library;
    }
    return choice;
}

std::optional<CatalogVersion> GuestLibraryCatalog::selectedVersion(
    const std::string &name) const {
    const auto *lib = select(name, std::nullopt, 0);
    if (!lib) return std::nullopt;
    return lib->version;
}

std::optional<std::uint64_t> GuestLibraryCatalog::resolve(
    const std::string &library, const std::string &symbol,
    std::optional<std::uint16_t> major, std::uint16_t minimumMinor) const {
    const auto *lib = select(library, major, minimumMinor);
    if (!lib) return std::nullopt;
    const auto found = lib->exports.find(symbol);
    if (found == lib->exports.end()) return std::nullopt;
    return found->second;
}

std::optional<ModuleRegistry> GuestLibraryCatalog::snapshot() const {
    ModuleRegistry registry;
    for (const auto &library : libraries_) {
        const auto *best = select(library.name, std::nullopt, 0);
        if (best != &library) continue;
        GuestModule module{library.name, {}};
        for (const auto &item : library.exports)
            module.exports.emplace(item.first, item.second);
        if (!registry.registerModule(std::move(module))) return std::nullopt;
    }
    return registry;
}

std::optional<DynamicLinkSummary> loadAndLinkCatalogELF(
    const std::uint8_t *elf, std::size_t length, std::uint64_t bias,
    const GuestLibraryCatalog &catalog, GuestMemory &memory) {
    const auto modules = catalog.snapshot();
    if (!modules) return std::nullopt;
    return loadAndLinkDynamicELF(elf, length, bias, *modules, memory);
}

std::vector<std::uint8_t> makeCatalogDynamicFixture() {
    auto elf = makeDynamicCPUFixture();
    // The original RW PT_LOAD maps offset 0x200..0x440. Move its end to 0x4B8
    // to hold five more RELA records; addend targets remain near the GOT.
    elf.resize(0x4B8, 0);
    put(elf, 152, 0x2B8, 8); // PT_LOAD p_filesz
    put(elf, 160, 0x2B8, 8); // PT_LOAD p_memsz
    put(elf, 0x240 + 6 * 16 + 8, 7 * 24, 8); // DT_RELASZ (index 6)

    // Existing 2 relocs: RELATIVE at 0x1108; JUMP_SLOT at 0x1100.
    // Additional five: ABS64, 32, 32S, PC32, PLT32.
    const std::uint64_t types[] = {1, 10, 11, 2, 4};
    const std::uint64_t addresses[] = {0x1110, 0x1120, 0x1128, 0x1130, 0x1138};
    const std::int64_t addends[] = {-8, 4, -4, -4, -4};
    for (unsigned i = 0; i < 5; ++i) {
        const auto at = 0x430 + i * 24;
        put(elf, at, addresses[i], 8);
        put(elf, at + 8, (std::uint64_t(1) << 32) | types[i], 8);
        put(elf, at + 16, static_cast<std::uint64_t>(addends[i]), 8);
    }
    return elf;
}

std::optional<CatalogExecutionDiagnostic> runCatalogDynamicDiagnostic() {
    GuestMemory memory;
    const std::uint8_t code[] = {0x48, 0x83, 0xC0, 0x02, 0xC3};
    if (!memory.map(0x9000, sizeof(code), permission::read | permission::write) ||
        !memory.writeBytes(0x9000, code, sizeof(code)) ||
        !memory.protect(0x9000, sizeof(code), permission::read | permission::execute))
        return std::nullopt;

    GuestLibraryCatalog catalog;
    if (!catalog.registerLibrary({"libMisakiDynamic", {1, 0}, {{"increment2", 0xA000}}}) ||
        !catalog.registerLibrary({"libMisakiDynamic", {1, 2}, {{"increment2", 0x9000}}}))
        return std::nullopt;
    const auto elf = makeCatalogDynamicFixture();
    const auto linked = loadAndLinkCatalogELF(elf.data(), elf.size(), 0x4000,
                                               catalog, memory);
    if (!linked || linked->relativeRelocations != 1 ||
        linked->importedSymbols != 6 || linked->absoluteRelocations != 3 ||
        linked->pcRelativeRelocations != 2 || linked->firstImportTarget != 0x9000)
        return std::nullopt;
    const auto absolute64 = memory.read64(0x5110);
    const auto absolute32 = memory.read64(0x5120); // Low 4 bytes are relocated.
    const auto signed32 = memory.read64(0x5128);
    const auto relative32 = memory.read64(0x5130);
    const auto plt32 = memory.read64(0x5138);
    if (!absolute64 || !absolute32 || !signed32 || !relative32 || !plt32)
        return std::nullopt;
    // This is read from unsigned little-endian storage without host pointer casts.
    const std::uint32_t as32 = static_cast<std::uint32_t>(*absolute32);
    const std::int32_t ss32 = static_cast<std::int32_t>(static_cast<std::uint32_t>(*signed32));
    const std::int32_t pc32 = static_cast<std::int32_t>(static_cast<std::uint32_t>(*relative32));
    const std::int32_t pl32 = static_cast<std::int32_t>(static_cast<std::uint32_t>(*plt32));
    if (*absolute64 != 0x8FF8 || as32 != 0x9004 || ss32 != 0x8FFC ||
        pc32 != 0x3ECC || pl32 != 0x3EC4) return std::nullopt;
    if (!memory.protect(0x5100, 0x2B8, permission::read)) return std::nullopt;
    const bool readOnly = !memory.write64(0x5100, 0);
    PortableX64Backend backend(std::move(memory), linked->entry);
    CatalogExecutionDiagnostic report;
    report.execution = backend.run(24);
    report.linked = *linked;
    report.catalogVersions = static_cast<std::uint32_t>(catalog.versionCount());
    report.absolute64 = *absolute64;
    report.unsigned32 = as32;
    report.signed32 = ss32;
    report.relative32 = pc32;
    report.plt32 = pl32;
    report.importReadOnly = readOnly;
    return report;
}

} // namespace misaki
