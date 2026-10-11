#include "../Include/MisakiMega55Bridge.h"
#include "../Include/GuestSCE55.hpp"
extern "C" int32_t misaki_mega55_diagnostic(MisakiMega55Report *report){
    if(!report)return -1;
    *report=MisakiMega55Report{};
    try {
        const auto r=misaki::runSCE55Diagnostic();
        if(!r)return -2;
        report->abi_version=1;
        report->pass_51=r->stages[0];report->pass_52=r->stages[1];
        report->pass_53=r->stages[2];report->pass_54=r->stages[3];
        report->pass_55=r->stages[4];
        report->elf_segments=r->elfSegments;
        report->elf_relocations=r->elfRelocations;
        report->self_encrypted_segments=r->selfEncryptedSegments;
        report->sce_dynamic_tags=r->sceDynamicTags;
        report->synthetic_nids=r->syntheticNids;
        report->processes=r->processes;
        report->mapped_pages=r->mappedPages;
        report->pm4_packets=r->pm4Packets;
        report->gcn_lanes=r->gcnLanes;
        report->gcn_instructions=r->gcnInstructions;
        report->output_index_7=r->outputIndex7;
        report->guest_rax=r->guestRax;
        report->shader_hash=r->shaderHash;
        report->encrypted_self_refused=r->encryptedSelfRefused;
        report->firmware_blockers=r->firmwareBlockers;
        report->firmware_attempted=r->realFirmwareAttempted;
        report->home_menu_booted=r->homeMenuBooted;
        if(!report->pass_51 || !report->pass_52 || !report->pass_53 ||
           !report->pass_54 || !report->pass_55 || !report->encrypted_self_refused ||
           !report->shader_hash || report->firmware_blockers!=5 ||
           report->firmware_attempted || report->home_menu_booted) return -3;
        return 0;
    } catch(...) {return -2;}
}
