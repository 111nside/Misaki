#include "../Include/GuestGCN49.hpp"
#include <vector>
#include <utility>
namespace misaki {
std::optional<GCN49Report> submitGCN49GuestFrame(
    GuestMemory &memory,std::uint64_t present,std::string *error){
    if(error)error->clear();
    const auto reject=[&](const char* msg)->std::optional<GCN49Report>{
        if(error)*error=msg;return std::nullopt;};
    if(present==0 || present>UINT64_MAX-gcn49FrameBytes)
        return reject("Invalid guest display buffer address");
    const auto fixture=makePM4IndirectFixture();
    const auto state=analyzePM4DrawStates(memory,fixture.root,fixture.rootWords.size(),error);
    if(!state || state->draws.size()!=1 ||
       state->draws[0].status!=PM4DrawStatus::metadataReady ||
       state->draws[0].colorAddress>UINT64_MAX-gcn49FrameBytes ||
       (present < state->draws[0].colorAddress+gcn49FrameBytes &&
        state->draws[0].colorAddress < present+gcn49FrameBytes))
        return reject("Unsupported PM4 target or presentation alias");
    GuestMemory staged=memory;
    const auto frame=executeGCNMemoryStage(staged,fixture.root,fixture.rootWords.size(),
                                             g21QueueAddress,10,1,error);
    if(!frame)return std::nullopt;
    std::vector<std::uint8_t> pixels(gcn49FrameBytes);
    for(std::size_t i=0;i<pixels.size();++i){
        const auto byte=staged.read8(frame->targetAddress+i);
        if(!byte)return reject("Rendered guest target is unreadable");
        pixels[i]=*byte;
    }
    if(!staged.writeBytes(present,pixels.data(),pixels.size()) ||
       !staged.protect(present,pixels.size(),permission::read))
        return reject("Presentation buffer absent, protected or incorrectly sized");
    if(staged.writeBytes(present,pixels.data(),1) ||
       staged.read8(present)!=staged.read8(frame->targetAddress) ||
       staged.read8(present+gcn49FrameBytes-1)!=
       staged.read8(frame->targetAddress+gcn49FrameBytes-1))
        return reject("Presentation buffer seal or copy verification failed");
    memory=std::move(staged);
    GCN49Report r;
    r.pm4Packets=frame->pm4Packets;r.draws=static_cast<std::uint32_t>(state->draws.size());
    r.sprites=frame->sprites;r.targetAddress=frame->targetAddress;
    r.presentAddress=present;r.fence=frame->completedFence;
    r.checksum=frame->framebufferChecksum;r.verifiedCopy=true;r.presentationReadOnly=true;
    return r;
}
}
