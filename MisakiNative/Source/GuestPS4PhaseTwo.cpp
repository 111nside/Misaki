#include "../Include/GuestPS4PhaseTwo.hpp"
#include <algorithm>
#include <utility>
namespace misaki {
namespace {
void mov(std::vector<std::uint8_t> &code, unsigned reg, std::uint64_t v) {
    code.push_back(0x48); code.push_back(std::uint8_t(0xb8 + reg));
    for (unsigned i=0;i<8;++i) code.push_back(std::uint8_t(v>>(8*i)));
}
std::vector<std::uint8_t> bootProgram() {
    std::vector<std::uint8_t> c;
    mov(c,0,sce_shim::pid); c.insert(c.end(),{0x0f,0x05});
    mov(c,0,sce_shim::clock); c.insert(c.end(),{0x0f,0x05});
    mov(c,7,0x300000); mov(c,6,5);
    mov(c,0,sce_shim::write); c.insert(c.end(),{0x0f,0x05});
    mov(c,7,0x150000);
    c.insert(c.end(),{0x48,0x8B,0x07,0xFF,0xE0}); // MOV RAX,[RDI]; JMP RAX
    return c;
}
}
std::optional<PS4PhaseTwoDiagnostic> runPS4PhaseTwoDiagnostic() {
    try {
        PS4PhaseTwoDiagnostic out;
        GuestMemory memory;
        auto image=makeSCEStagingFixture();
        const auto code=bootProgram();
        if (code.size()>0x70) return std::nullopt;
        std::copy(code.begin(),code.end(),image.begin()+0x200);
        auto staged=stageSCEELFFixture(image.data(),image.size(),0x100000,
                                       {{0x1078,0x2000}},memory);
        if (!staged || staged->entry!=0x101000 || staged->relativeRelocations!=1 ||
            memory.read64(0x101078)!=0x102000) return std::nullopt;
        out.mappedSegments=staged->mappedSegments;
        out.relocations=staged->relativeRelocations;
        out.relocationValue=staged->lastRelocationValue;
        out.stagePassed[0]=true;
        if (!installPS4SyntheticImportFunctions(memory) ||
            !memory.map(0x150000,0x40,permission::read|permission::write))
            return std::nullopt;
        auto catalog=makePS4SyntheticImportCatalog(memory);
        if (!catalog) return std::nullopt;
        auto bound=bindSCEImportSlots(*catalog,
            {{makePS4SyntheticKey(0),0x150000},
             {makePS4SyntheticKey(1),0x150008}},0x150000,0x40,memory);
        if (!bound || bound->bindings!=2 || memory.read64(0x150000)!=0x9000 ||
            memory.write64(0x150000,0)) return std::nullopt;
        out.imports=bound->bindings;
        out.stagePassed[1]=true;
        constexpr std::uint8_t text[]={'R','e','a','d','y'};
        if (!memory.map(0x300000,sizeof(text),permission::read|permission::write) ||
            !memory.writeBytes(0x300000,text,sizeof(text)) ||
            !memory.protect(0x300000,sizeof(text),permission::read))
            return std::nullopt;
        SCEKernelShim services(1001);
        PortableX64Backend cpu(memory,staged->entry,&services,1001);
        const auto result=cpu.run(128);
        if (result.stop!=X64Stop::halted || result.rax()!=42 ||
            !result.stackRestored() || services.calls()!=3 || services.failures()!=0 ||
            services.lastPID()!=1001 || services.lastTicks()!=1000 ||
            services.output()!=std::vector<std::uint8_t>(text,text+5)) return std::nullopt;
        out.serviceCalls=services.calls();
        out.guestInstructions=result.state.instructions;
        out.emittedBytes=static_cast<std::uint32_t>(services.output().size());
        out.guestResult=result.rax();
        out.stagePassed[2]=true;
        if (!installGCNMemoryStageFixture(memory)) return std::nullopt;
        const auto fixture=makePM4IndirectFixture();
        const auto gpu=executeGCNMemoryStage(memory,fixture.root,fixture.rootWords.size(),
                                               g21QueueAddress,10,1);
        if (!gpu || gpu->pm4Packets!=11 || gpu->sprites!=2 ||
            gpu->completedFence!=1 || gpu->targetAddress!=0x20000000)
            return std::nullopt;
        out.pm4Packets=gpu->pm4Packets;
        out.gpuPackets=gpu->gpuPackets;
        out.sprites=gpu->sprites;
        out.completedFence=gpu->completedFence;
        out.framebufferChecksum=gpu->framebufferChecksum;
        out.stagePassed[3]=true;
        const auto readiness=runPS4BootReadinessDiagnostic();
        if (!readiness || readiness->blockers.size()!=5 ||
            readiness->firmwareExecutionAttempted || readiness->homeMenuBooted ||
            out.framebufferChecksum==0) return std::nullopt;
        out.bootBlockers=static_cast<std::uint32_t>(readiness->blockers.size());
        out.stagePassed[4]=true; // an accurate blocked-boot diagnostic
        return out;
    } catch (...) { return std::nullopt; }
}
} // namespace misaki
