#include "../Include/MisakiCore.hpp"

#include <algorithm>
#include <limits>
#include <utility>

namespace misaki {
namespace {
constexpr std::size_t maxFile = 16 * 1024 * 1024;
constexpr std::size_t maxMapping = 8 * 1024 * 1024;

std::uint16_t u16(const std::uint8_t *bytes, std::size_t at) {
    return std::uint16_t(bytes[at]) |
           (std::uint16_t(bytes[at + 1]) << 8);
}
std::uint32_t u32(const std::uint8_t *bytes, std::size_t at) {
    std::uint32_t result = 0;
    for (std::size_t i = 0; i < 4; ++i)
        result |= std::uint32_t(bytes[at + i]) << (8 * i);
    return result;
}
std::uint64_t u64(const std::uint8_t *bytes, std::size_t at) {
    std::uint64_t result = 0;
    for (std::size_t i = 0; i < 8; ++i)
        result |= std::uint64_t(bytes[at + i]) << (8 * i);
    return result;
}
void writeLE(std::vector<std::uint8_t> &buffer, std::size_t at,
             std::uint64_t value, std::size_t count) {
    for (std::size_t i = 0; i < count; ++i)
        buffer[at + i] = static_cast<std::uint8_t>(value >> (8 * i));
}
void reportError(std::string *error, const char *message) {
    if (error != nullptr) *error = message;
}
} // namespace

std::optional<ELFInfo> inspectELF64(const std::uint8_t *bytes, std::size_t size,
                                    std::string *error) {
    if (!bytes || size < 64 || size > maxFile) {
        reportError(error, "Invalid ELF file size");
        return std::nullopt;
    }
    if (bytes[0] != 0x7f || bytes[1] != 'E' || bytes[2] != 'L' || bytes[3] != 'F' ||
        bytes[4] != 2 || bytes[5] != 1 || bytes[6] != 1) {
        reportError(error, "Only little-endian ELF64 is supported");
        return std::nullopt;
    }
    const auto type = u16(bytes, 16);
    const auto machine = u16(bytes, 18);
    if ((type != 2 && type != 3) || machine != 0x3e) {
        reportError(error, "Expected x86-64 ET_EXEC or ET_DYN");
        return std::nullopt;
    }
    const auto table = u64(bytes, 32);
    const auto width = u16(bytes, 54);
    const auto count = u16(bytes, 56);
    if (count == 0 || count > 128 || width < 56 || width > 4096 ||
        table > size || count > (size - static_cast<std::size_t>(table)) / width) {
        reportError(error, "Invalid ELF program-header table");
        return std::nullopt;
    }
    ELFInfo info;
    info.type = type;
    info.machine = machine;
    info.entryPoint = u64(bytes, 24);
    for (std::size_t i = 0; i < count; ++i) {
        const auto at = static_cast<std::size_t>(table) + i * width;
        const auto kind = u32(bytes, at);
        if (kind == 2) info.hasDynamicSegment = true;
        if (kind != 1) continue;
        const auto fileOffset = u64(bytes, at + 8);
        const auto fileSize = u64(bytes, at + 32);
        const auto memSize = u64(bytes, at + 40);
        const auto virtualAddress = u64(bytes, at + 16);
        if (fileOffset > size || fileSize > size - fileOffset ||
            memSize < fileSize || memSize > maxMapping ||
            virtualAddress > std::numeric_limits<std::uint64_t>::max() - memSize) {
            reportError(error, "Invalid ELF load segment");
            return std::nullopt;
        }
        ++info.loadSegments;
    }
    if (info.loadSegments == 0) {
        reportError(error, "No PT_LOAD segment");
        return std::nullopt;
    }
    return info;
}

bool GuestMemory::map(std::uint64_t start, std::size_t length, std::uint8_t flags) {
    if (length == 0 || length > maxMapping || flags > 7 ||
        start > std::numeric_limits<std::uint64_t>::max() - length) return false;
    const auto end = start + length;
    auto next = regions_.lower_bound(start);
    if (next != regions_.end() && next->first < end) return false;
    if (next != regions_.begin()) {
        auto prev = std::prev(next);
        if (prev->second.bytes.size() > start - prev->first) return false;
    }
    regions_.emplace(start, Region{std::vector<std::uint8_t>(length, 0), flags});
    return true;
}

bool GuestMemory::protect(std::uint64_t start, std::size_t length, std::uint8_t flags) {
    auto it = regions_.find(start);
    if (it == regions_.end() || it->second.bytes.size() != length || flags > 7) return false;
    it->second.flags = flags;
    return true;
}

const GuestMemory::Region *GuestMemory::find(std::uint64_t address, std::uint8_t needed) const {
    auto next = regions_.upper_bound(address);
    if (next == regions_.begin()) return nullptr;
    const auto it = std::prev(next);
    if (address - it->first >= it->second.bytes.size() ||
        (it->second.flags & needed) != needed) return nullptr;
    return &it->second;
}

GuestMemory::Region *GuestMemory::findWritable(std::uint64_t address) {
    auto next = regions_.upper_bound(address);
    if (next == regions_.begin()) return nullptr;
    const auto it = std::prev(next);
    if (address - it->first >= it->second.bytes.size() ||
        !(it->second.flags & permission::write)) return nullptr;
    return &it->second;
}

std::optional<std::uint8_t> GuestMemory::read8(std::uint64_t address) const {
    auto *region = find(address, permission::read);
    if (!region) return std::nullopt;
    auto it = regions_.upper_bound(address);
    auto begin = std::prev(it)->first;
    return region->bytes[static_cast<std::size_t>(address - begin)];
}

std::optional<std::uint8_t> GuestMemory::fetch8(std::uint64_t address) const {
    auto *region = find(address, permission::execute);
    if (!region) return std::nullopt;
    const auto start = std::prev(regions_.upper_bound(address))->first;
    return region->bytes[static_cast<std::size_t>(address - start)];
}

bool GuestMemory::writeBytes(std::uint64_t address, const std::uint8_t *source,
                            std::size_t length) {
    if ((!source && length != 0) || length > maxMapping ||
        (length != 0 && address > std::numeric_limits<std::uint64_t>::max() - (length - 1)))
        return false;
    for (std::size_t i = 0; i < length; ++i)
        if (!findWritable(address + i)) return false;
    for (std::size_t i = 0; i < length; ++i) {
        auto *region = findWritable(address + i);
        const auto start = std::prev(regions_.upper_bound(address + i))->first;
        region->bytes[static_cast<std::size_t>(address + i - start)] = source[i];
    }
    return true;
}

std::optional<std::uint64_t> GuestMemory::read64(std::uint64_t address) const {
    if (address > std::numeric_limits<std::uint64_t>::max() - 7) return std::nullopt;
    std::uint64_t result = 0;
    for (unsigned i = 0; i < 8; ++i) {
        auto byte = read8(address + i);
        if (!byte) return std::nullopt;
        result |= std::uint64_t(*byte) << (8 * i);
    }
    return result;
}

bool GuestMemory::write64(std::uint64_t address, std::uint64_t value) {
    if (address > std::numeric_limits<std::uint64_t>::max() - 7) return false;
    // Preflight all bytes to prevent partial writes on a protection fault.
    for (unsigned i = 0; i < 8; ++i)
        if (!findWritable(address + i)) return false;
    for (unsigned i = 0; i < 8; ++i) {
        auto *region = findWritable(address + i);
        const auto base = std::prev(regions_.upper_bound(address + i))->first;
        region->bytes[static_cast<std::size_t>(address + i - base)] =
            static_cast<std::uint8_t>(value >> (8 * i));
    }
    return true;
}

bool GuestMemory::isExecutable(std::uint64_t address) const {
    return find(address, permission::execute) != nullptr;
}

bool ModuleRegistry::registerModule(GuestModule module) {
    if (module.name.empty() || modules_.count(module.name)) return false;
    for (const auto &pair : module.exports)
        if (pair.first.empty() || pair.second == 0) return false;
    const auto name = module.name;
    return modules_.emplace(name, std::move(module)).second;
}

std::optional<std::uint64_t> ModuleRegistry::resolve(const std::string &module,
                                                      const std::string &symbol) const {
    auto found = modules_.find(module);
    if (found == modules_.end()) return std::nullopt;
    auto exported = found->second.exports.find(symbol);
    if (exported == found->second.exports.end()) return std::nullopt;
    return exported->second;
}

bool ModuleRegistry::bindImport(GuestMemory &memory, std::uint64_t slot,
                                const std::string &module, const std::string &symbol) const {
    const auto address = resolve(module, symbol);
    return address && memory.write64(slot, *address);
}

std::vector<std::uint8_t> makeDiagnosticELF() {
    // Tiny synthetic ET_DYN header and one PT_LOAD. No code or PS4 firmware.
    std::vector<std::uint8_t> file(0x104, 0);
    file[0] = 0x7f; file[1] = 'E'; file[2] = 'L'; file[3] = 'F';
    file[4] = 2; file[5] = 1; file[6] = 1;
    writeLE(file, 16, 3, 2);     // ET_DYN
    writeLE(file, 18, 0x3e, 2);  // x86-64
    writeLE(file, 20, 1, 4);
    writeLE(file, 24, 0x20, 8);  // entry offset (metadata only)
    writeLE(file, 32, 64, 8);
    writeLE(file, 52, 64, 2);
    writeLE(file, 54, 56, 2);
    writeLE(file, 56, 1, 2);
    writeLE(file, 64, 1, 4);     // PT_LOAD
    writeLE(file, 68, 5, 4);     // RX
    writeLE(file, 72, 0x100, 8);
    writeLE(file, 80, 0x0, 8);
    writeLE(file, 96, 4, 8);
    writeLE(file, 104, 0x100, 8);
    return file;
}

std::optional<Diagnostic> runDiagnostic() {
    const auto file = makeDiagnosticELF();
    const auto elf = inspectELF64(file.data(), file.size());
    if (!elf) return std::nullopt;
    GuestMemory memory;
    if (!memory.map(0x4000, 16, permission::read | permission::write)) return std::nullopt;
    ModuleRegistry modules;
    if (!modules.registerModule({"libMisakiDemo", {{"return42", 0x6200}}}))
        return std::nullopt;
    if (!modules.bindImport(memory, 0x4000, "libMisakiDemo", "return42"))
        return std::nullopt;
    if (!memory.protect(0x4000, 16, permission::read)) return std::nullopt;
    const auto linked = memory.read64(0x4000);
    if (!linked || *linked != 0x6200) return std::nullopt;
    Diagnostic result;
    result.elf = *elf;
    result.loadedModules = static_cast<std::uint32_t>(modules.count());
    result.importsResolved = 1;
    result.linkedAddress = *linked;
    result.importReadOnly = !memory.write64(0x4000, 0);
    return result;
}
} // namespace misaki
