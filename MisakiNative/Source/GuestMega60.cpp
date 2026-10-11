#include "../Include/GuestMega60.hpp"
#include "../Include/GuestPS4Formats.hpp"
namespace misaki {
std::optional<Mega60Result> runMega60Diagnostic(){
    const auto fixture=makeSCE56Fixture();
    const auto d=inspectSCE56Dynamic(fixture.data(),fixture.size());
    // Independent reads from the same self-authored bytes, never a Sony file.
    const auto s=runService57Diagnostic();
    const auto g=runGuestGPU58Diagnostic();
    const auto p=runPersistent59Diagnostic();
    const auto self=makePS4StructuralFixture(true);
    const auto a=analyzeExecutable60(self.data(),self.size(),self.size());
    Mega60Result r;
    r.stages[0]=d&&d->neededCount==1&&d->relativeTableEntries==1&&d->relocationTableConsistent;
    r.stages[1]=s&&s->resolved==2&&s->calls==3&&s->failures==1&&s->processIsolated;
    r.stages[2]=g&&g->pixels==256&&g->copies==1&&g->fence==1&&g->rejectedInvalid;
    r.stages[3]=p&&p->processes==2&&p->exited==2&&p->calls==8&&p->yields==2;
    r.stages[4]=a.kind==Compat60Kind::self&&a.encryptedSegments==1&&a.encryptedRefused&&!a.canExecute;
    if(!r.stages[0]||!r.stages[1]||!r.stages[2]||!r.stages[3]||!r.stages[4])return std::nullopt;
    r.elfDependencies=d->neededCount;r.relaRecords=d->relativeTableEntries;
    r.services=s->calls;r.serviceFailures=s->failures;
    r.gpuPixels=g->pixels;r.gpuCopies=g->copies;
    r.guestProcesses=p->processes;r.guestExits=p->exited;
    r.guestInstructions=p->instructions;r.contextSwitches=p->contextSwitches;
    r.encryptedSelfSegments=a.encryptedSegments;
    r.unmetFirmwarePrerequisites=5;
    r.importedAnalysisRefusedExecution=true;
    r.bootAttempted=false;r.homeMenuBooted=false;
    return r;
}
} // namespace misaki
