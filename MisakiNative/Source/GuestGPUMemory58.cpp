#include "../Include/GuestGPUMemory58.hpp"
#include <array>
namespace misaki {
bool GuestGPUMemory58::submit(std::uint32_t pid,std::uint64_t in,std::uint64_t out,
                               std::uint32_t w,std::uint32_t h,std::uint64_t fence){
    // Tiny guest RGBA8 surfaces only; no real GPU command execution.
    if(!w||!h||w>64||h>64||w*h>4096||fence==0||fence<=completed_||
       fence!=completed_+1||in%4||out%4||in==out)return false;
    const std::size_t n=std::size_t(w)*h*4;
    if(in>UINT64_MAX-n||out>UINT64_MAX-n ||
       !(out+n<=in || in+n<=out))return false;
    std::vector<std::uint8_t> pixels;
    pixels.reserve(n);
    for(std::size_t i=0;i<n;++i){
        auto byte=kernel_.read(pid,in+i);
        if(!byte)return false;
        pixels.push_back(*byte);
    }
    // A single GuestVM48::write is transactional; failure cannot partially copy.
    if(!kernel_.write(pid,out,pixels.data(),pixels.size()))return false;
    std::uint64_t hash=14695981039346656037ull;
    for(auto b:pixels){hash^=b;hash*=1099511628211ull;}
    completed_=fence;++copies_;checksum_=hash;
    return true;
}
std::optional<GuestGPU58Report> runGuestGPU58Diagnostic(){
    GuestKernel53 kernel;
    const auto p=kernel.createProcess();
    if(!p||!kernel.map(*p,0x800000,4096,permission::read|permission::write)||
       !kernel.map(*p,0x810000,4096,permission::read|permission::write))return std::nullopt;
    std::vector<std::uint8_t> data(16*16*4);
    for(std::size_t i=0;i<data.size();++i)data[i]=static_cast<std::uint8_t>((i*7)&255);
    if(!kernel.write(*p,0x800000,data.data(),data.size())||
       !kernel.protect(*p,0x800000,4096,permission::read))return std::nullopt;
    auto other=kernel.cloneProcess(*p);
    if(!other)return std::nullopt;
    GuestGPUMemory58 gpu(kernel);
    if(!gpu.submit(*p,0x800000,0x810000,16,16,1) ||
       kernel.read(*p,0x810000)!=data[0] ||
       kernel.read(*p,0x810000+31)!=data[31] ||
       kernel.read(*other,0x810000)!=0)return std::nullopt;
    if(gpu.submit(*other,0x800000,0x810000,16,16,1) ||
       gpu.submit(*p,0x800000,0x810000,0,16,2) ||
       gpu.submit(*p,0x800000,0x800008,16,16,2) ||
       gpu.submit(*p,0x800000,0x810000,100,100,2))return std::nullopt;
    if(!kernel.protect(*p,0x810000,4096,permission::read) ||
       gpu.submit(*p,0x800000,0x810000,16,16,2))return std::nullopt;
    GuestGPU58Report r;
    r.width=16;r.height=16;r.pixels=256;
    r.copies=gpu.copies();r.fence=gpu.completedFence();r.checksum=gpu.checksum();
    r.processIsolated=true;r.rejectedInvalid=true;
    return r;
}
} // namespace misaki
