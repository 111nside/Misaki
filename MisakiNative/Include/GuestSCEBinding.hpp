#pragma once
#include "GuestPS4Imports.hpp"
#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <vector>
namespace misaki {
// Exact-key, transactional binding of *synthetic* SCE-like NID imports into
// guest GOT slots. Never binds to iOS host code or authentic Sony exports.
struct SCEImportSlot { PS4ImportKey symbol; std::uint64_t address = 0; };
struct SCEBoundSlots {
    std::uint32_t bindings = 0;
    std::uint64_t firstAddress = 0;
    bool sealedReadOnly = false;
};
std::optional<SCEBoundSlots> bindSCEImportSlots(
    const PS4ImportCatalog &catalog, const std::vector<SCEImportSlot> &slots,
    std::uint64_t regionStart, std::size_t regionBytes, GuestMemory &memory,
    std::string *error = nullptr);
} // namespace misaki
