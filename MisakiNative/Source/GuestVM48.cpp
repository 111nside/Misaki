#include "../Include/GuestVM48.hpp"
#include <algorithm>
#include <utility>
namespace misaki {
namespace {
constexpr std::uint64_t kPage=4096;
constexpr std::uint64_t kMaximum=2*1024*1024;
bool pageRange(std::uint64_t address,std::size_t bytes){
    return address>=kPage && (address%kPage)==0 && bytes>=kPage &&
           bytes<=kMaximum && bytes%kPage==0 && address<=UINT64_MAX-bytes;
}
}
bool GuestVM48::allowed(std::uint8_t f){
    return (f&permission::read) && !(f & ~std::uint8_t(7)) &&
           !(f&permission::write && f&permission::execute);
}
bool GuestVM48::rebuild(const std::vector<Region>&regions,GuestMemory &out){
    GuestMemory scratch;
    for(const auto&r:regions){
        if(!scratch.map(r.start,r.data.size(),permission::read|permission::write) ||
           !scratch.writeBytes(r.start,r.data.data(),r.data.size()) ||
           !scratch.protect(r.start,r.data.size(),r.flags))return false;
    }
    out=std::move(scratch);return true;
}
bool GuestVM48::map(std::uint64_t start,std::size_t n,std::uint8_t f){
    if(!pageRange(start,n) || !allowed(f) || maps_.size()>=32 ||
       n>kMaximum-mapped_)return false;
    std::vector<Region> staged=maps_;
    staged.push_back({start,f,std::vector<std::uint8_t>(n,0)});
    GuestMemory scratch;
    if(!rebuild(staged,scratch))return false;
    maps_=std::move(staged);memory_=std::move(scratch);mapped_+=n;return true;
}
bool GuestVM48::protect(std::uint64_t start,std::size_t n,std::uint8_t f){
    if(!pageRange(start,n) || !allowed(f))return false;
    auto staged=maps_;
    auto it=std::find_if(staged.begin(),staged.end(),[&](const Region&r){
        return r.start==start && r.data.size()==n;});
    if(it==staged.end())return false;
    it->flags=f;
    GuestMemory scratch;
    if(!rebuild(staged,scratch))return false;
    maps_=std::move(staged);memory_=std::move(scratch);return true;
}
bool GuestVM48::unmap(std::uint64_t start,std::size_t n){
    if(!pageRange(start,n))return false;
    auto staged=maps_;
    auto it=std::find_if(staged.begin(),staged.end(),[&](const Region&r){
        return r.start==start && r.data.size()==n;});
    if(it==staged.end())return false;
    staged.erase(it);
    GuestMemory scratch;
    if(!rebuild(staged,scratch))return false;
    maps_=std::move(staged);memory_=std::move(scratch);mapped_-=n;return true;
}
bool GuestVM48::write(std::uint64_t at,const std::uint8_t *data,std::size_t n){
    if(!data || n==0 || n>65536 || at>UINT64_MAX-n)return false;
    auto staged=maps_;
    for(auto &r:staged){
        if((r.flags&permission::write) && at>=r.start &&
           n<=r.data.size() && at-r.start<=r.data.size()-n){
            std::copy(data,data+n,r.data.begin()+static_cast<std::size_t>(at-r.start));
            GuestMemory scratch=memory_;
            if(!scratch.writeBytes(at,data,n))return false;
            maps_=std::move(staged);memory_=std::move(scratch);return true;
        }
    }
    return false;
}
std::optional<std::uint8_t> GuestVM48::read(std::uint64_t at)const {
    return memory_.read8(at);
}
}
