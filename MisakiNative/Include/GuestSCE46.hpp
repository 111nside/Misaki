#pragma once
#include "GuestSCEStaging.hpp"
#include <cstdint>
#include <cstddef>
#include <optional>
#include <string>
#include <vector>

namespace misaki {
// Bounded, unencrypted, SELF-free SCE ELF64 fixture staging. Standard ELF
// PT_DYNAMIC/DT_RELA/R_X86_64_RELATIVE only, not Sony's proprietary relocations.
struct SCE46Image {
    std::uint32_t segments = 0;
    std::uint32_t relocations = 0;
    std::uint64_t entry = 0;
    std::uint64_t relocationValue = 0;
    std::uint64_t imageBytes = 0;
};
std::optional<SCE46Image> loadSCE46(const std::uint8_t *data, std::size_t length,
                                    std::uint64_t bias, GuestMemory &memory,
                                    std::string *error = nullptr);
std::vector<std::uint8_t> makeSCE46Fixture();
}
