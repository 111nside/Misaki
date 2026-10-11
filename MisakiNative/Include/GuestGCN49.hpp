#pragma once
#include "GuestGCNMemoryStage.hpp"
#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
namespace misaki {
// PM4 state -> protected guest target -> sealed guest presentation buffer.
// Actual pixel generation remains Misaki's software 2D demo, NOT GCN rasterization.
struct GCN49Report {
    std::uint32_t pm4Packets=0,draws=0,sprites=0;
    std::uint64_t targetAddress=0,presentAddress=0;
    std::uint64_t fence=0,checksum=0;
    bool verifiedCopy=false,presentationReadOnly=false;
};
constexpr std::uint64_t gcn49PresentationAddress=0x22000000;
constexpr std::size_t gcn49FrameBytes=std::size_t(g21Width)*g21Height*4;
std::optional<GCN49Report> submitGCN49GuestFrame(
    GuestMemory &memory,std::uint64_t presentAddress=gcn49PresentationAddress,
    std::string *error=nullptr);
} // namespace misaki
