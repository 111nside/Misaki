#include "../Include/GuestKernel53.hpp"
#include <array>
namespace misaki {
std::optional<std::uint32_t> GuestKernel53::createProcess(){
    if(processes_.size()>=8 || nextHandle_==UINT32_MAX)return std::nullopt;
    const auto handle=nextHandle_++;
    processes_.emplace(handle,GuestVM48{});
    return handle;
}
std::optional<std::uint32_t> GuestKernel53::cloneProcess(std::uint32_t parent){
    auto it=processes_.find(parent);
    if(it==processes_.end() || processes_.size()>=8 || nextHandle_==UINT32_MAX)
        return std::nullopt;
    const auto handle=nextHandle_++;
    processes_.emplace(handle,it->second); // independent GuestMemory snapshot
    return handle;
}
bool GuestKernel53::terminate(std::uint32_t handle){return processes_.erase(handle)==1;}
bool GuestKernel53::map(std::uint32_t handle,std::uint64_t address,
                        std::size_t bytes,std::uint8_t flags){
    auto it=processes_.find(handle);
    return it!=processes_.end() && it->second.map(address,bytes,flags);
}
bool GuestKernel53::protect(std::uint32_t handle,std::uint64_t address,
                            std::size_t bytes,std::uint8_t flags){
    auto it=processes_.find(handle);
    return it!=processes_.end() && it->second.protect(address,bytes,flags);
}
bool GuestKernel53::unmap(std::uint32_t handle,std::uint64_t address,std::size_t bytes){
    auto it=processes_.find(handle);
    return it!=processes_.end() && it->second.unmap(address,bytes);
}
bool GuestKernel53::write(std::uint32_t handle,std::uint64_t address,
                          const std::uint8_t *bytes,std::size_t count){
    auto it=processes_.find(handle);
    return it!=processes_.end() && it->second.write(address,bytes,count);
}
std::optional<std::uint8_t> GuestKernel53::read(std::uint32_t handle,
                                                std::uint64_t address)const{
    auto it=processes_.find(handle);
    return it==processes_.end()?std::nullopt:it->second.read(address);
}
std::size_t GuestKernel53::pages(std::uint32_t handle)const{
    auto it=processes_.find(handle);
    return it==processes_.end()?0:it->second.mappedBytes()/4096;
}
std::optional<GuestKernel53Report> runGuestKernel53Diagnostic(){
    GuestKernel53 kernel;
    const auto parent=kernel.createProcess();
    const std::array<std::uint8_t,4> original{{1,2,3,4}};
    const std::array<std::uint8_t,4> changed{{9,8,7,6}};
    if(!parent || !kernel.map(*parent,0x800000,4096,permission::read|permission::write) ||
       !kernel.write(*parent,0x800000,original.data(),original.size()))return std::nullopt;
    const auto child=kernel.cloneProcess(*parent);
    if(!child || !kernel.write(*child,0x800000,changed.data(),changed.size()) ||
       kernel.read(*parent,0x800000)!=1 || kernel.read(*child,0x800000)!=9 ||
       !kernel.protect(*child,0x800000,4096,permission::read) ||
       kernel.write(*child,0x800000,original.data(),original.size()))return std::nullopt;
    GuestKernel53Report r;
    r.parent=*parent;r.child=*child;
    r.isolated=true;r.sealedReadOnly=true;
    r.mappedPages=static_cast<std::uint32_t>(kernel.pages(*parent)+kernel.pages(*child));
    if(!kernel.terminate(*child) || kernel.read(*child,0x800000) ||
       kernel.read(*parent,0x800000)!=1)return std::nullopt;
    r.terminated=true;r.activeProcesses=static_cast<std::uint32_t>(kernel.processCount());
    return r;
}
} // namespace misaki
