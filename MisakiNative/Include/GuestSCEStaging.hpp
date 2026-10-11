#pragma once
#include "GuestPS4Formats.hpp"
#include "MisakiCore.hpp"
#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace misaki {
// A strict, transactional SCE-ELF staging experiment for SELF-FREE, original
// unencrypted test fixtures.  No encrypted SELF extraction, Sony relocation
// tables, real module startup, or signed executable support is implied.
struct SCERelativePatch {
    std::uint64_t virtualAddress = 0;
    std::uint64_t addend = 0;
};
struct SCEStagedImage {
    std::uint64_t entry = 0;
    std::uint64_t loadBias = 0;
    std::uint32_t mappedSegments = 0;
    std::uint32_t relativeRelocations = 0;
    std::uint64_t lastRelocationValue = 0;
};
std::optional<SCEStagedImage> stageSCEELFFixture(
    const std::uint8_t *bytes, std::size_t length, std::uint64_t bias,
    const std::vector<SCERelativePatch> &patches, GuestMemory &memory,
    std::string *error = nullptr);
std::vector<std::uint8_t> makeSCEStagingFixture();
} // namespace misaki
