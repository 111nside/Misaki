#pragma once
#include "GuestKernel53.hpp"
#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <vector>
namespace misaki {
// Scoped per-process software GPU surface copies. This is NOT GCN DMA/MMU,
// VRAM tiling, texture format emulation, or Sony's GNM API.
class GuestGPUMemory58 {
public:
    explicit GuestGPUMemory58(GuestKernel53 &kernel):kernel_(kernel){}
    bool submit(std::uint32_t process,std::uint64_t input,std::uint64_t output,
                std::uint32_t width,std::uint32_t height,std::uint64_t fence);
    std::uint64_t completedFence()const{return completed_;}
    std::uint32_t copies()const{return copies_;}
    std::uint64_t checksum()const{return checksum_;}
private:
    GuestKernel53 &kernel_;
    std::uint64_t completed_=0,checksum_=0;
    std::uint32_t copies_=0;
};
struct GuestGPU58Report {std::uint32_t width=0,height=0,pixels=0,copies=0;
    std::uint64_t fence=0,checksum=0;bool processIsolated=false,rejectedInvalid=false;};
std::optional<GuestGPU58Report> runGuestGPU58Diagnostic();
} // namespace misaki
