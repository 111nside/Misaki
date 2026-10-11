#pragma once
#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <vector>
namespace misaki {
enum class Compat60Kind : std::uint32_t {unknown=0,sceELF=1,self=2,ordinaryELF=3};
struct Compat60Finding {
    std::uint32_t code=0; // 1 invalid,2 encrypted,3 compressed,4 unsupported format,
                          // 5 unknown dependencies,6 missing PS4 services,
                          // 7 incomplete GPU,8 no firmware boot,9 partial inspection,
                          // 10 unimplemented load/relocations
    std::string description;
};
struct Compat60Result {
    Compat60Kind kind=Compat60Kind::unknown;
    std::uint16_t elfType=0;
    std::uint32_t loadSegments=0,dynamicSegments=0,encryptedSegments=0,compressedSegments=0;
    std::uint64_t inspectedBytes=0,declaredBytes=0;
    bool partial=false,formatRecognized=false,encryptedRefused=false,canExecute=false;
    std::vector<std::string> neededModules;
    std::vector<Compat60Finding> findings;
    std::string summary;
};
// Read-only inspection of at most 8MiB, with separately supplied *actual* file
// length. A partial prefix never authorizes execution or declares a file valid.
Compat60Result analyzeExecutable60(const std::uint8_t *bytes,std::size_t prefixLength,
                                  std::uint64_t actualSize);
} // namespace misaki
