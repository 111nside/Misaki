#pragma once
#include "GuestSCE51.hpp"
#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace misaki {
// Read-only ELF dynamic metadata. The returned strings are guest-declared
// dependencies, NOT verified Sony modules or host paths.
struct SCE56Dynamic {
    std::uint32_t entries=0, neededCount=0, sceModuleCount=0;
    std::uint32_t relativeTableEntries=0, loadSegments=0;
    std::vector<std::string> neededNames;
    std::vector<std::uint64_t> sceNeededIdentifiers;
    bool hasStringTable=false, relocationTableConsistent=false;
    bool hasUnsupportedRelocations=false;
};
std::optional<SCE56Dynamic> inspectSCE56Dynamic(const std::uint8_t *data,
      std::size_t length,std::string *error=nullptr);
std::vector<std::uint8_t> makeSCE56Fixture();
} // namespace misaki
