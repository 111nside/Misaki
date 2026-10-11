#pragma once
#include "GuestFusedGCN30.hpp"
#include "GuestPM4State.hpp"
#include <array>
#include <cstdint>
#include <optional>
#include <string>
namespace misaki {
// Interleaves PM4 command-state inspection with bounded per-lane GCN MUBUF
// execution against the SAME protected guest address space. This is still an
// intentionally small software interpreter, not AMD wavefront or rasterization.
struct GCN54Report {
    std::uint32_t pm4Packets=0,draws=0;
    std::uint32_t laneMask=0,lanes=0,instructions=0;
    std::uint64_t shaderAddress=0,colorTarget=0,outputHash=0;
    std::array<std::uint32_t,4> loaded{},indices{};
    std::array<std::uint32_t,64> output{};
    bool shaderReadOnly=false,inputReadOnly=false,descriptorsReadOnly=false;
};
std::optional<GCN54Report> runGCN54(std::uint32_t laneMask=15,
                                    std::string *error=nullptr);
} // namespace misaki
