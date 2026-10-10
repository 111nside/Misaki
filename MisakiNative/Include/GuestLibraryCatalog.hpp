#pragma once

#include "GuestDynamicExecution.hpp"

#include <cstddef>
#include <cstdint>
#include <map>
#include <optional>
#include <string>
#include <vector>

namespace misaki {

// Versioned name-based test-library registry. This is not the PS4 NID resolver,
// SCE module manager, or a substitute for licensed system libraries.
struct CatalogVersion {
    std::uint16_t major = 0;
    std::uint16_t minor = 0;
    bool operator==(const CatalogVersion &other) const {
        return major == other.major && minor == other.minor;
    }
    bool operator<(const CatalogVersion &other) const {
        return major < other.major || (major == other.major && minor < other.minor);
    }
};

struct CatalogLibrary {
    std::string name;
    CatalogVersion version;
    std::map<std::string, std::uint64_t> exports;
};

class GuestLibraryCatalog {
public:
    // Each name/version pair is unique. A newer version may coexist with an old one.
    // No guest ELF or host module is loaded implicitly by registration.
    bool registerLibrary(CatalogLibrary library);
    std::size_t versionCount() const { return libraries_.size(); }
    std::size_t libraryCount() const;
    std::optional<CatalogVersion> selectedVersion(const std::string &name) const;
    std::optional<std::uint64_t> resolve(const std::string &library,
                                         const std::string &symbol,
                                         std::optional<std::uint16_t> major = std::nullopt,
                                         std::uint16_t minimumMinor = 0) const;
    // Capture the highest available version of each library for ordinary
    // ELF DT_NEEDED names. This does NOT implement ELF symbol version sections.
    std::optional<ModuleRegistry> snapshot() const;

private:
    std::vector<CatalogLibrary> libraries_;
    const CatalogLibrary *select(const std::string &name,
                                  std::optional<std::uint16_t> major,
                                  std::uint16_t minimumMinor) const;
};

// A convenient integration path for the existing strict ELF dynamic loader.
// A snapshot is chosen before relocation; missing imports fail transactionally.
std::optional<DynamicLinkSummary> loadAndLinkCatalogELF(
    const std::uint8_t *elf, std::size_t length, std::uint64_t bias,
    const GuestLibraryCatalog &catalog, GuestMemory &memory);

struct CatalogExecutionDiagnostic {
    X64ExecutionResult execution;
    DynamicLinkSummary linked;
    std::uint32_t catalogVersions = 0;
    std::uint64_t absolute64 = 0;
    std::uint32_t unsigned32 = 0;
    std::int32_t signed32 = 0;
    std::int32_t relative32 = 0;
    std::int32_t plt32 = 0;
    bool importReadOnly = false;
};

// Original ELF fixture with five additional relocation kinds. The CPU actually
// calls the selected library and returns 42, not a mocked bridge return value.
std::vector<std::uint8_t> makeCatalogDynamicFixture();
std::optional<CatalogExecutionDiagnostic> runCatalogDynamicDiagnostic();

} // namespace misaki
