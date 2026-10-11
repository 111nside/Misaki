#pragma once
#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace misaki {
// Structural inspection of PS4-style SELF and SCE ELF64 metadata only.
// No authentication, decryption, decompression, relocation, or firmware loading.
enum class PS4ContainerKind : std::uint32_t { sceELF = 1, self = 2 };
struct PS4ExecutableMetadata {
    PS4ContainerKind kind = PS4ContainerKind::sceELF;
    std::uint16_t elfType = 0;
    std::uint16_t machine = 0;
    std::uint16_t programHeaders = 0;
    std::uint16_t selfSegments = 0;
    std::uint32_t encryptedSegments = 0;
    std::uint32_t compressedSegments = 0;
    std::uint64_t elfHeaderOffset = 0;
    std::uint64_t entry = 0;
    bool structuralOnly = true;
    bool readyToExecute = false;
};
std::optional<PS4ExecutableMetadata> inspectPS4Executable(
    const std::uint8_t *bytes, std::size_t size, std::string *error = nullptr);
// Entirely self-authored header fixture. It is not firmware or a genuine signed SELF.
std::vector<std::uint8_t> makePS4StructuralFixture(bool encrypted = false);
}
