#include "../Include/GuestBootCoordinator.hpp"
#include <utility>
namespace misaki {
std::optional<EarlyBootDiagnostic> runEarlyBootDiagnostic() {
    try {
        EarlyBootDiagnostic r;
        const auto self = makePS4StructuralFixture(true);
        const auto image = inspectPS4Executable(self.data(), self.size());
        if (!image || image->kind != PS4ContainerKind::self ||
            image->elfType != 0xFE10 || image->encryptedSegments != 1 ||
            image->readyToExecute) return std::nullopt;
        r.executable = *image;
        r.events.push_back({BootCheckPhase::selfHeader,true,
            "Synthetic SELF + SCE ELF headers recognized; encrypted payload is not executable"});
        GuestMemory libraryMemory;
        if (!installPS4SyntheticImportFunctions(libraryMemory)) return std::nullopt;
        const auto catalog = makePS4SyntheticImportCatalog(libraryMemory);
        if (!catalog) return std::nullopt;
        const auto bound = catalog->resolveAll({makePS4SyntheticKey(0),makePS4SyntheticKey(1)});
        if (!bound || bound->size()!=2 || (*bound)[0]!=0x9000 || (*bound)[1]!=0x9010)
            return std::nullopt;
        r.resolvedImports = static_cast<std::uint32_t>(bound->size());
        r.events.push_back({BootCheckPhase::syntheticImports,true,
            "Two synthetic SCE-shaped NIDs resolved to isolated guest RX functions"});
        const auto lanes = runGCNLaneMemoryDiagnostic();
        if (!lanes || lanes->executedLanes != 4 || lanes->instructions != 44 ||
            lanes->output[7] != 31 || !lanes->inputProtected) return std::nullopt;
        r.shaderLanes = lanes->executedLanes;
        r.shaderOperations = lanes->instructions;
        r.events.push_back({BootCheckPhase::shaderMemory,true,
            "Four cooperative test lanes completed bounded GCN load/store operations"});
        const auto session = runIntegratedMegaDiagnostic();
        if (!session || !session->stage31 || !session->stage32 || !session->stage33 ||
            !session->stage34 || !session->stage35 || !session->runtime.completed ||
            session->runtime.processes.size()!=2) return std::nullopt;
        const auto &p = session->runtime.processes.front();
        if (p.framebufferSprites != 2 || p.pm4Packets != 11 ||
            p.completedFence != 1 || p.pendingGPU != 0) return std::nullopt;
        r.sessionProcesses = static_cast<std::uint32_t>(session->runtime.processes.size());
        r.sessionInstructions = session->runtime.instructions;
        r.frameSprites = p.framebufferSprites;
        r.pm4Packets = p.pm4Packets;
        r.lastFence = p.completedFence;
        r.events.push_back({BootCheckPhase::integratedSession,true,
            "Two isolated toy guest processes, guest filesystem, GPU fences, PM4 and GCN"});
        // No genuine SCE kernel, firmware ELF, encrypted SELF extraction,
        // GPU wavefront semantics or launch is implemented.
        r.events.push_back({BootCheckPhase::realFirmware,false,
            "BLOCKED: PS4 kernel, real SELF loading, firmware and full GCN are unsupported"});
        r.syntheticDiagnosticsPassed = true;
        r.firmwareLaunched = false;
        return r;
    } catch (...) { return std::nullopt; }
}
}
