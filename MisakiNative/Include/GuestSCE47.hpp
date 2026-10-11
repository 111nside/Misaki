#pragma once
#include "GuestSCEBinding.hpp"
#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <vector>
namespace misaki {
// Dependency-ordered synthetic SCE module import plan. NID tokens here are
// original test labels, not Sony functions, library implementations or exports.
struct SCE47Module {
    std::string name;
    std::uint16_t major = 0, minor = 0;
    std::vector<std::string> dependencies;
    std::vector<SCEImportSlot> slots;
    std::uint64_t region = 0;
    std::size_t regionBytes = 0;
};
struct SCE47Result {
    std::vector<std::string> loadOrder;
    std::uint32_t imports = 0;
    std::uint32_t modules = 0;
    bool sealed = false;
};
std::optional<SCE47Result> bindSCE47Graph(const std::vector<SCE47Module>&modules,
    const PS4ImportCatalog &catalog,GuestMemory &memory,std::string *error=nullptr);
std::vector<SCE47Module> makeSCE47Modules();
}
