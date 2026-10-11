#pragma once
#include "MisakiCore.hpp"
#include <cstdint>
#include <map>
#include <optional>
#include <string>
#include <vector>
namespace misaki {
// Synthetic SCE-shaped imports, NOT a Sony library implementation or syscall map.
struct PS4ImportKey {
    std::string module;
    std::string library;
    std::string nid; // 11-character encoded NID token
    std::uint16_t major = 0, minor = 0;
    bool operator<(const PS4ImportKey &other) const;
};
struct PS4ImportBinding { PS4ImportKey key; std::uint64_t guestAddress = 0; };
class PS4ImportCatalog {
public:
    bool define(const GuestMemory &memory, PS4ImportBinding binding);
    std::optional<std::uint64_t> resolve(const PS4ImportKey &key) const;
    std::optional<std::vector<std::uint64_t>> resolveAll(
        const std::vector<PS4ImportKey> &requests) const;
    std::size_t size() const { return entries_.size(); }
private:
    std::map<PS4ImportKey, std::uint64_t> entries_;
};
PS4ImportKey makePS4SyntheticKey(std::uint32_t which);
bool installPS4SyntheticImportFunctions(GuestMemory &memory);
std::optional<PS4ImportCatalog> makePS4SyntheticImportCatalog(const GuestMemory &memory);
}
