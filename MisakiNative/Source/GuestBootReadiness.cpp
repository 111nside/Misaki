#include "../Include/GuestBootReadiness.hpp"
namespace misaki {
PS4BootReadiness evaluatePS4BootReadiness(const EarlyBootDiagnostic &d,
                                          const PS4BootCapabilities &c) {
    PS4BootReadiness out;
    for (const auto &step : d.events)
        if (step.phase != BootCheckPhase::realFirmware && step.passed)
            ++out.completedSyntheticChecks;
    if (!d.syntheticDiagnosticsPassed || out.completedSyntheticChecks != 4)
        out.blockers.push_back("Integrated synthetic diagnostic incomplete");
    if (!c.realSelfPayloadLoader)
        out.blockers.push_back("Encrypted PS4 SELF payload loader is not implemented");
    if (!c.realSceKernelServices)
        out.blockers.push_back("Real SCE kernel and system-library services are not implemented");
    if (!c.comprehensiveGCNWavefront)
        out.blockers.push_back("Complete PS4 GCN wavefront / shader execution is not implemented");
    if (!c.ps4FirmwareImagesProvided)
        out.blockers.push_back("Validated user-provided PS4 firmware components are not available");
    if (!c.ps4FirmwareVirtualMemory)
        out.blockers.push_back("PS4 firmware virtual memory and boot process are not implemented");
    out.prerequisitesReported = out.blockers.empty();
    // Even if all flags were set by an external caller, this diagnostic does
    // not itself launch a PS4 program or assert that a firmware boot happened.
    out.firmwareExecutionAttempted = false;
    out.homeMenuBooted = false;
    return out;
}
std::optional<PS4BootReadiness> runPS4BootReadinessDiagnostic() {
    const auto d = runEarlyBootDiagnostic();
    if (!d) return std::nullopt;
    return evaluatePS4BootReadiness(*d, PS4BootCapabilities{});
}
}
