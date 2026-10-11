#include "../Include/MisakiMega60Bridge.h"
#include "../Include/GuestMega60.hpp"
#include <cstring>
extern "C" int32_t misaki_mega60_diagnostic(MisakiMega60Report *out){
    if(!out)return -1;
    *out=MisakiMega60Report{};
    try{
        const auto r=misaki::runMega60Diagnostic();
        if(!r)return -2;
        out->abi_version=1;
        out->pass_56=r->stages[0];out->pass_57=r->stages[1];
        out->pass_58=r->stages[2];out->pass_59=r->stages[3];out->pass_60=r->stages[4];
        out->elf_dependencies=r->elfDependencies;out->rela_records=r->relaRecords;
        out->service_calls=r->services;out->service_failures=r->serviceFailures;
        out->gpu_pixels=r->gpuPixels;out->gpu_copies=r->gpuCopies;
        out->guest_processes=r->guestProcesses;out->guest_exits=r->guestExits;
        out->guest_instructions=r->guestInstructions;out->context_switches=r->contextSwitches;
        out->encrypted_self_segments=r->encryptedSelfSegments;
        out->firmware_blockers=r->unmetFirmwarePrerequisites;
        out->analysis_refused_execution=r->importedAnalysisRefusedExecution;
        out->firmware_attempted=r->bootAttempted;out->home_menu_booted=r->homeMenuBooted;
        return 0;
    }catch(...){return -3;}
}
extern "C" int32_t misaki_compat60_inspect(const uint8_t *bytes,size_t prefix,
       uint64_t actual,MisakiCompatibility60Report *report,char *text,size_t capacity){
    if(!bytes||prefix<4||prefix>8*1024*1024||actual<prefix||!report||
       !text||!capacity)return -1;
    try{
        const auto r=misaki::analyzeExecutable60(bytes,prefix,actual);
        if(r.summary.size()+1>capacity)return -2;
        MisakiCompatibility60Report stage{};
        stage.abi_version=1;
        stage.kind=static_cast<uint32_t>(r.kind);stage.elf_type=r.elfType;
        stage.load_segments=r.loadSegments;stage.dynamic_segments=r.dynamicSegments;
        stage.encrypted_segments=r.encryptedSegments;
        stage.compressed_segments=r.compressedSegments;
        stage.needed_modules=static_cast<uint32_t>(r.neededModules.size());
        stage.findings=static_cast<uint32_t>(r.findings.size());
        stage.partial=r.partial;stage.recognized=r.formatRecognized;
        stage.execution_refused=!r.canExecute;
        stage.inspected_bytes=r.inspectedBytes;stage.file_bytes=r.declaredBytes;
        std::memcpy(text,r.summary.c_str(),r.summary.size()+1);
        *report=stage;
        return 0;
    }catch(...){return -3;}
}
