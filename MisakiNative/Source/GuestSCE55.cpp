#include "../Include/GuestSCE55.hpp"
#include <array>
namespace misaki {
std::optional<SCE55Report> runSCE55Diagnostic(){
    try {
        SCE55Report r;
        // 51: container inspection refuses an encrypted SELF as loadable.
        const auto encrypted=makePS4StructuralFixture(true);
        const auto self=inspectSCE51(encrypted.data(),encrypted.size());
        const auto image=makeSCE46Fixture();
        const auto elf=inspectSCE51(image.data(),image.size());
        if(!self || !elf || self->disposition!=SCE51Disposition::encryptedPayload ||
           self->encryptedSegments!=1 || self->firmwareExecutable ||
           elf->container!=PS4ContainerKind::sceELF || elf->loadSegments!=2 ||
           elf->dynamicSegments!=1)return std::nullopt;
        r.selfEncryptedSegments=self->encryptedSegments;
        r.encryptedSelfRefused=true;
        r.stages[0]=true;
        // 52: read bounded SCE-compatible dynamic table, parse NID structure,
        // and match exactly two synthetic exports; nothing is bound to iOS.
        auto tags=inspectSCE52Dynamic(image.data(),image.size());
        if(!tags || tags->entries!=3 || !tags->ended)return std::nullopt;
        GuestMemory symbols;
        if(!installPS4SyntheticImportFunctions(symbols))return std::nullopt;
        auto catalog=makePS4SyntheticImportCatalog(symbols);
        if(!catalog)return std::nullopt;
        std::vector<PS4ImportKey> requests{makePS4SyntheticKey(0),makePS4SyntheticKey(1)};
        for(const auto &k:requests){
            const auto symbol=parseSCE52Symbol(k.nid+"#"+k.library+"#"+k.module);
            if(!symbol || symbol->encodedNid!=k.nid)return std::nullopt;
        }
        const auto nids=resolveSCE52(requests,*catalog);
        if(!nids || nids->matched!=2 || nids->firstAddress!=0x9000)return std::nullopt;
        r.sceDynamicTags=tags->entries;r.syntheticNids=nids->matched;r.stages[1]=true;
        // 53: make two independent virtual address spaces and guard writes.
        const auto kernel=runGuestKernel53Diagnostic();
        if(!kernel || !kernel->isolated || !kernel->sealedReadOnly ||
           !kernel->terminated || kernel->mappedPages!=2)return std::nullopt;
        r.processes=kernel->activeProcesses;r.mappedPages=kernel->mappedPages;
        r.stages[2]=true;
        // 54: decode PM4 and run the restricted MUBUF shader on 4 lanes in
        // the same isolated GPU memory snapshot, checking output bytes.
        const auto gpu=runGCN54(15);
        if(!gpu || gpu->pm4Packets!=11 || gpu->draws!=1 || gpu->lanes!=4 ||
           !gpu->shaderReadOnly || !gpu->inputReadOnly ||
           !gpu->descriptorsReadOnly || gpu->output[7]!=31)return std::nullopt;
        r.pm4Packets=gpu->pm4Packets;r.gcnLanes=gpu->lanes;
        r.gcnInstructions=gpu->instructions;
        r.outputIndex7=gpu->output[7];r.shaderHash=gpu->outputHash;
        r.stages[3]=true;
        // 55: connect the prior ELF + services + PM4 session and the new
        // compatibility checks. Never claim to have executed real firmware.
        const auto runtime=runSCE50Diagnostic();
        if(!runtime || runtime->firmwareBlockers!=5 || runtime->firmwareAttempted ||
           runtime->homeMenuBooted || runtime->segments!=2 ||
           runtime->relocations!=1 || runtime->guestRax!=42 ||
           runtime->completedFence!=1)return std::nullopt;
        r.elfSegments=runtime->segments;r.elfRelocations=runtime->relocations;
        r.guestRax=runtime->guestRax;r.firmwareBlockers=runtime->firmwareBlockers;
        r.realFirmwareAttempted=false;r.homeMenuBooted=false;
        r.stages[4]=true;
        return r;
    }catch(...) {return std::nullopt;}
}
} // namespace misaki
