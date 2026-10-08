#pragma once

#include <cstddef>
#include <cstdint>
#include <map>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

namespace misaki {

// This is a portable test engine. These are NOT Sony or FreeBSD kernel APIs.
namespace permission {
constexpr std::uint8_t read = 1;
constexpr std::uint8_t write = 2;
constexpr std::uint8_t execute = 4;
}

struct ELFInfo {
    std::uint16_t type = 0;
    std::uint16_t machine = 0;
    std::uint16_t loadSegments = 0;
    std::uint64_t entryPoint = 0;
    bool hasDynamicSegment = false;
};

// Validates a bounded, ordinary ELF64 header and program-header table.
// It does not parse PlayStation SELF/PRX containers or run guest instructions.
std::optional<ELFInfo> inspectELF64(const std::uint8_t *data,
                                    std::size_t size,
                                    std::string *error = nullptr);

class GuestMemory {
public:
    bool map(std::uint64_t start, std::size_t length, std::uint8_t flags);
    bool protect(std::uint64_t start, std::size_t length, std::uint8_t flags);
    std::optional<std::uint8_t> read8(std::uint64_t address) const;
    std::optional<std::uint8_t> fetch8(std::uint64_t address) const;
    bool writeBytes(std::uint64_t address, const std::uint8_t *source, std::size_t length);
    std::optional<std::uint64_t> read64(std::uint64_t address) const;
    bool write64(std::uint64_t address, std::uint64_t value);
    bool isExecutable(std::uint64_t address) const;

private:
    struct Region {
        std::vector<std::uint8_t> bytes;
        std::uint8_t flags;
    };
    std::map<std::uint64_t, Region> regions_;
    const Region *find(std::uint64_t address, std::uint8_t needed) const;
    Region *findWritable(std::uint64_t address);
};

struct GuestModule {
    std::string name;
    std::unordered_map<std::string, std::uint64_t> exports;
};

// Guest symbol registry with transactional pointer patching.
class ModuleRegistry {
public:
    bool registerModule(GuestModule module);
    std::optional<std::uint64_t> resolve(const std::string &module,
                                          const std::string &symbol) const;
    bool bindImport(GuestMemory &memory, std::uint64_t slot,
                    const std::string &module, const std::string &symbol) const;
    std::size_t count() const { return modules_.size(); }
private:
    std::unordered_map<std::string, GuestModule> modules_;
};

std::vector<std::uint8_t> makeDiagnosticELF();

struct Diagnostic {
    ELFInfo elf;
    std::uint32_t loadedModules = 0;
    std::uint32_t importsResolved = 0;
    std::uint64_t linkedAddress = 0;
    bool importReadOnly = false;
};

std::optional<Diagnostic> runDiagnostic();

} // namespace misaki
