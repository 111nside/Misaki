#include "../Include/GuestSCE50.hpp"
#include <array>
#include <vector>
namespace misaki {
std::optional<SCE50Report> runSCE50Diagnostic(){
    try {
        SCE50Report r;
        GuestMemory memory;
        // 46: stage multiple SCE-style ELF segments and ordinary DT_RELA.
        const auto elf=makeSCE46Fixture();
        const auto staged=loadSCE46(elf.data(),elf.size(),0x100000,memory);
        if(!staged || staged->segments!=2 || staged->relocations!=1 ||
           memory.read64(0x103010)!=0x103500)return std::nullopt;
        PortableX64Backend first(memory,staged->entry);
        const auto firstRun=first.run(40);
        if(firstRun.stop!=X64Stop::halted || firstRun.rax()!=42)return std::nullopt;
        r.segments=staged->segments;r.relocations=staged->relocations;
        r.relocationValue=staged->relocationValue;r.guestRax=firstRun.rax();
        r.stages[0]=true;
        // 47: dependency-ordered synthetic NID binding within the same VM.
        if(!installPS4SyntheticImportFunctions(memory) ||
           !memory.map(0x150000,32,permission::read|permission::write) ||
           !memory.map(0x151000,32,permission::read|permission::write))return std::nullopt;
        const auto catalog=makePS4SyntheticImportCatalog(memory);
        if(!catalog)return std::nullopt;
        const auto linked=bindSCE47Graph(makeSCE47Modules(),*catalog,memory);
        if(!linked || linked->modules!=2 || linked->imports!=2 ||
           memory.read64(0x150000)!=0x9000 || memory.read64(0x151000)!=0x9010)
            return std::nullopt;
        r.modules=linked->modules;r.imports=linked->imports;r.stages[1]=true;
        // 48: isolate a guest IO page using the new page-granular manager.
        GuestVM48 io;
        const std::array<std::uint8_t,5> message{{'R','e','a','d','y'}};
        if(!io.map(0x300000,4096,permission::read|permission::write) ||
           !io.write(0x300000,message.data(),message.size()) ||
           !io.protect(0x300000,4096,permission::read) ||
           io.snapshot().read8(0x300000)!=message[0] ||
           !memory.map(0x300000,4096,permission::read|permission::write) ||
           !memory.writeBytes(0x300000,message.data(),message.size()) ||
           !memory.protect(0x300000,4096,permission::read))return std::nullopt;
        r.mappedGuestPages=static_cast<std::uint32_t>(io.mappedBytes()/4096);
        r.stages[2]=true;
        // 43-style diagnostic services are not actual sceKernel calls.
        auto serviceProgram=makeSCEKernelGuestFixture();
        if(!memory.map(0x180000,serviceProgram.size(),permission::read|permission::write) ||
           !memory.writeBytes(0x180000,serviceProgram.data(),serviceProgram.size()) ||
           !memory.protect(0x180000,serviceProgram.size(),permission::read|permission::execute))
            return std::nullopt;
        SCEKernelShim services(1001);
        PortableX64Backend cpu(memory,0x180000,&services,1001);
        const auto executed=cpu.run(100);
        if(executed.stop!=X64Stop::exited || services.calls()!=4 ||
           services.failures()!=0 || services.output()!=
           std::vector<std::uint8_t>(message.begin(),message.end()))return std::nullopt;
        r.serviceCalls=services.calls();r.serviceFailures=services.failures();
        // 49: inspect PM4 state, stage software pixels, and seal guest display.
        if(!installGCNMemoryStageFixture(memory) ||
           !memory.map(gcn49PresentationAddress,gcn49FrameBytes,
                       permission::read|permission::write))return std::nullopt;
        const auto frame=submitGCN49GuestFrame(memory);
        if(!frame || frame->pm4Packets!=11 || frame->sprites!=2 ||
           !frame->verifiedCopy || !frame->presentationReadOnly)return std::nullopt;
        r.pm4Packets=frame->pm4Packets;r.sprites=frame->sprites;
        r.framebufferChecksum=frame->checksum;r.completedFence=frame->fence;
        r.stages[3]=true;
        // 50: report authentic firmware prerequisites; never execute firmware.
        const auto readiness=runPS4BootReadinessDiagnostic();
        if(!readiness || readiness->prerequisitesReported ||
           readiness->blockers.size()!=5 || readiness->firmwareExecutionAttempted ||
           readiness->homeMenuBooted)return std::nullopt;
        r.firmwareBlockers=static_cast<std::uint32_t>(readiness->blockers.size());
        r.firmwareAttempted=readiness->firmwareExecutionAttempted;
        r.homeMenuBooted=readiness->homeMenuBooted;
        r.stages[4]=true;
        return r;
    } catch (...) {return std::nullopt;}
}
}
