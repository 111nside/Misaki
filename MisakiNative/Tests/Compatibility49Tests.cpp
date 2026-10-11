#include "GuestGCN49.hpp"
#include <cassert>
#include <iostream>
using namespace misaki;
int main(){
    GuestMemory memory;
    assert(installGCNMemoryStageFixture(memory));
    assert(memory.map(gcn49PresentationAddress,gcn49FrameBytes,permission::read|permission::write));
    const auto result=submitGCN49GuestFrame(memory);
    assert(result && result->pm4Packets==11 && result->draws==1 && result->sprites==2);
    assert(result->targetAddress==0x20000000 && result->presentAddress==gcn49PresentationAddress);
    assert(result->checksum!=0 && result->fence==1 && result->verifiedCopy && result->presentationReadOnly);
    assert(memory.read8(gcn49PresentationAddress)==0x0c && !memory.write64(gcn49PresentationAddress,7));
    auto bad=memory;
    assert(!submitGCN49GuestFrame(bad)); // presentation already read-only
    assert(bad.read8(gcn49PresentationAddress)==0x0c);
    GuestMemory noMirror;
    assert(installGCNMemoryStageFixture(noMirror));
    assert(!submitGCN49GuestFrame(noMirror));
    assert(noMirror.read8(0x20000000)==0); // transactional no frame commit
    GuestMemory blocked;
    assert(installGCNMemoryStageFixture(blocked));
    assert(blocked.map(gcn49PresentationAddress,gcn49FrameBytes,permission::read|permission::write));
    assert(blocked.protect(0x20000000,gcn49FrameBytes,permission::read));
    assert(!submitGCN49GuestFrame(blocked));
    assert(blocked.read8(gcn49PresentationAddress)==0);
    assert(!submitGCN49GuestFrame(blocked,UINT64_MAX-2));
    std::cout<<"PASS: compatibility 49 PM4 target-to-guest-frame presentation\n";
}
