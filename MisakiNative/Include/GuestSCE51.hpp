#pragma once
#include "GuestPS4Formats.hpp"
#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace misaki {
// Read-only PS4 SELF/SCE-ELF metadata inspection; never decrypts or executes SELF.
// A file can be structurally recognized but still entirely unsupported for loading.
enum class SCE51Disposition : std::uint32_t {
    unencryptedFixtureOnly = 1, encryptedPayload = 2, compressedPayload = 3,
    unsupportedLayout = 4
};
struct SCE51Segment {
    std::uint32_t type = 0, flags = 0;
    std::uint64_t offset = 0, virtualAddress = 0, fileBytes = 0, memoryBytes = 0;
};
struct SCE51Report {
    PS4ContainerKind container = PS4ContainerKind::sceELF;
    SCE51Disposition disposition = SCE51Disposition::unsupportedLayout;
    std::uint16_t elfType = 0;
    std::uint32_t loadSegments = 0, specialSegments = 0, dynamicSegments = 0;
    std::uint32_t encryptedSegments = 0, compressedSegments = 0;
    std::uint32_t executableSegments = 0;
    std::uint64_t entry = 0;
    std::vector<SCE51Segment> segments;
    bool structuralOnly = true;
    bool firmwareExecutable = false;
};
std::optional<SCE51Report> inspectSCE51(const std::uint8_t *bytes,
                                        std::size_t size, std::string *error = nullptr);
} // namespace misaki
