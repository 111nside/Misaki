#pragma once
#include "GuestPS4Imports.hpp"
#include "GuestSCE47.hpp"
#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <vector>
namespace misaki {
// Parse PS4-style NID symbol names of the form NID#Library#Module.
// This is symbol-name structure only, not Sony NID derivation or an export database.
struct SCE52Symbol {
    std::string encodedNid, library, module;
};
std::optional<SCE52Symbol> parseSCE52Symbol(const std::string &name);
// Inspect SCE dynamic tags from an unwrapped SCE ELF's file-backed PT_DYNAMIC.
// All values remain metadata; none is dereferenced as a native address.
struct SCE52Tags {
    std::uint32_t entries=0, sceTags=0, neededModules=0, importLibraries=0;
    std::uint64_t moduleInfo=0, firstNeeded=0;
    bool ended=false;
};
std::optional<SCE52Tags> inspectSCE52Dynamic(const std::uint8_t *bytes,
                                            std::size_t size, std::string *error=nullptr);
struct SCE52Resolution {
    std::uint32_t matched=0;
    std::uint64_t firstAddress=0;
};
// Strictly resolve explicit synthetic exports; never maps NIDs to host functions.
std::optional<SCE52Resolution> resolveSCE52(
    const std::vector<PS4ImportKey> &keys, const PS4ImportCatalog &catalog);
} // namespace misaki
