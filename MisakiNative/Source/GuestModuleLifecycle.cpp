#include "../Include/GuestModuleLifecycle.hpp"
#include "../Include/GuestCPU.hpp"

#include <algorithm>
#include <functional>
#include <limits>
#include <utility>

namespace misaki {
namespace {
constexpr std::size_t maxDefinitions = 32;
constexpr std::size_t maxImage = 64 * 1024;
}

bool GuestModuleLifecycle::validName(const std::string &name) {
    if (name.empty() || name.size() > 96 || name == "." || name == ".." ||
        name.find("..") != std::string::npos) return false;
    for (unsigned char c : name)
        if (!((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
              (c >= '0' && c <= '9') || c == '_' || c == '-' || c == '.'))
            return false;
    return true;
}

bool GuestModuleLifecycle::define(GuestModuleDefinition module) {
    if (!validName(module.name) || definitions_.size() >= maxDefinitions ||
        definitions_.count(module.name) || module.image.empty() ||
        module.image.size() > maxImage || module.needed.size() > maxDefinitions ||
        module.exports.size() > 128 ||
        module.address > std::numeric_limits<std::uint64_t>::max() - module.image.size())
        return false;
    std::set<std::string> seen;
    for (const auto &dep : module.needed)
        if (!validName(dep) || !seen.insert(dep).second) return false;
    for (const auto &item : module.exports) {
        if (!validName(item.first) || item.second < module.address ||
            item.second - module.address >= module.image.size()) return false;
    }
    const auto name = module.name;
    definitions_.emplace(name, std::move(module));
    return true;
}

bool GuestModuleLifecycle::transition(
    const std::map<std::string, std::uint32_t> &nextRoots) {
    // Resolve every dependency before changing live state; cycles, missing
    // modules, overlaps, or failed allocations leave the old state untouched.
    std::map<std::string, std::uint8_t> visiting;
    std::vector<std::string> order;
    std::function<bool(const std::string &)> visit = [&](const std::string &name) {
        auto found = definitions_.find(name);
        if (found == definitions_.end()) return false;
        const auto status = visiting[name];
        if (status == 1) return false; // cycle
        if (status == 2) return true;
        visiting[name] = 1;
        for (const auto &dependency : found->second.needed)
            if (!visit(dependency)) return false;
        visiting[name] = 2;
        order.push_back(name);
        return true;
    };
    for (const auto &root : nextRoots)
        if (root.second != 0 && !visit(root.first)) return false;

    std::set<std::string> nextActive(order.begin(), order.end());
    std::map<std::string, std::uint32_t> nextReferences;
    for (const auto &name : nextActive)
        nextReferences[name] = nextRoots.count(name) ? nextRoots.at(name) : 0;
    for (const auto &name : nextActive) {
        for (const auto &dependency : definitions_.at(name).needed) {
            auto &count = nextReferences[dependency];
            if (count == std::numeric_limits<std::uint32_t>::max()) return false;
            ++count;
        }
    }

    // GuestMemory supports whole-region protection but not unmapping yet.
    // Rebuild the lifecycle-owned *immutable* code image transactionally;
    // this intentionally cannot be used for mutable guest process memory.
    GuestMemory candidate;
    for (const auto &name : order) {
        const auto &definition = definitions_.at(name);
        if (!candidate.map(definition.address, definition.image.size(),
                           permission::read | permission::write) ||
            !candidate.writeBytes(definition.address, definition.image.data(),
                                  definition.image.size()) ||
            !candidate.protect(definition.address, definition.image.size(),
                               permission::read | permission::execute)) return false;
    }

    // Finalize parents before dependencies, initialize dependencies before parents.
    std::vector<GuestModuleEvent> additions;
    // Finalization order is derived from the previous dependency graph.
    std::vector<std::string> oldOrder;
    std::set<std::string> oldVisited;
    std::function<void(const std::string &)> oldVisit = [&](const std::string &name) {
        if (!oldVisited.insert(name).second) return;
        for (const auto &dep : definitions_.at(name).needed)
            if (active_.count(dep)) oldVisit(dep);
        oldOrder.push_back(name);
    };
    for (const auto &name : active_) oldVisit(name);
    for (auto it = oldOrder.rbegin(); it != oldOrder.rend(); ++it)
        if (!nextActive.count(*it))
            additions.push_back({GuestModuleEvent::Kind::finalized, *it});
    for (const auto &name : order)
        if (!active_.count(name))
            additions.push_back({GuestModuleEvent::Kind::initialized, name});

    for (const auto &event : additions) {
        if (event.kind == GuestModuleEvent::Kind::initialized) ++initializations_;
        else ++finalizations_;
    }
    events_.insert(events_.end(), additions.begin(), additions.end());
    roots_ = nextRoots;
    references_ = std::move(nextReferences);
    active_ = std::move(nextActive);
    memory_ = std::move(candidate);
    return true;
}

bool GuestModuleLifecycle::acquire(const std::string &name) {
    if (!definitions_.count(name)) return false;
    auto next = roots_;
    auto &current = next[name];
    if (current == std::numeric_limits<std::uint32_t>::max()) return false;
    ++current;
    return transition(next);
}

bool GuestModuleLifecycle::release(const std::string &name) {
    const auto it = roots_.find(name);
    if (it == roots_.end() || it->second == 0) return false;
    auto next = roots_;
    if (--next[name] == 0) next.erase(name);
    return transition(next);
}

bool GuestModuleLifecycle::loaded(const std::string &name) const {
    return active_.count(name) != 0;
}
std::uint32_t GuestModuleLifecycle::references(const std::string &name) const {
    const auto it = references_.find(name);
    return it == references_.end() ? 0 : it->second;
}
std::uint32_t GuestModuleLifecycle::opens(const std::string &name) const {
    const auto it = roots_.find(name);
    return it == roots_.end() ? 0 : it->second;
}

std::optional<ModuleRegistry> GuestModuleLifecycle::activeRegistry() const {
    ModuleRegistry registry;
    for (const auto &name : active_) {
        const auto &definition = definitions_.at(name);
        if (definition.exports.empty()) continue;
        GuestModule module{name, {}};
        for (const auto &entry : definition.exports) module.exports.emplace(entry);
        if (!registry.registerModule(std::move(module))) return std::nullopt;
    }
    return registry;
}

std::optional<GuestModuleLifecycleDiagnostic> runGuestModuleLifecycleDiagnostic() {
    // Three independently authored, mapped RX code modules.
    GuestModuleLifecycle manager;
    const std::vector<std::uint8_t> base = {0x90, 0xC3}; // nop; ret
    const std::vector<std::uint8_t> math = {
        0x48, 0xB8, 40, 0, 0, 0, 0, 0, 0, 0, // mov rax,40
        0x48, 0x05, 2, 0, 0, 0,             // add rax,2
        0xC3                               // ret
    };
    const std::vector<std::uint8_t> app = {
        0x48, 0xFF, 0x15, 0xF9, 0, 0, 0, // call qword [rip+0xF9]
        0xF4                            // hlt
    };
    if (!manager.define({"libBase", 0x3000, base, {}, {}}) ||
        !manager.define({"libMath", 0x4000, math, {"libBase"}, { {"return42", 0x4000} }}) ||
        !manager.define({"guestApp", 0x1000, app, {"libMath"}, {}}) ||
        !manager.acquire("guestApp")) return std::nullopt;
    if (manager.loadedCount() != 3 || manager.references("libMath") != 1 ||
        manager.references("libBase") != 1 || manager.initializations() != 3)
        return std::nullopt;

    const auto registry = manager.activeRegistry();
    if (!registry) return std::nullopt;
    GuestMemory memory = manager.memory();
    if (!memory.map(0x1100, 8, permission::read | permission::write) ||
        !registry->bindImport(memory, 0x1100, "libMath", "return42") ||
        !memory.protect(0x1100, 8, permission::read)) return std::nullopt;
    const bool ro = !memory.write64(0x1100, 0);
    GuestCPU cpu(std::move(memory), 0x1000);
    const auto run = cpu.run(20);
    if (run.stop != GuestStop::halted || run.rax != 42 ||
        run.instructions != 5 || !run.stackRestored) return std::nullopt;

    if (!manager.acquire("guestApp") || manager.opens("guestApp") != 2 ||
        manager.initializations() != 3 || !manager.release("guestApp") ||
        manager.loadedCount() != 3 || manager.references("libBase") != 1 ||
        !manager.release("guestApp") || manager.loadedCount() != 0 ||
        manager.finalizations() != 3 || manager.memory().fetch8(0x4000))
        return std::nullopt;
    const auto emptyRegistry = manager.activeRegistry();
    const bool unloaded = !manager.loaded("guestApp") && !manager.loaded("libMath") &&
                          !manager.loaded("libBase") && emptyRegistry &&
                          !emptyRegistry->resolve("libMath", "return42");
    const bool protectedDependency = ro &&
        manager.events().size() == 6 &&
        manager.events()[0].name == "libBase" &&
        manager.events()[1].name == "libMath" &&
        manager.events()[2].name == "guestApp" &&
        manager.events()[3].name == "guestApp" &&
        manager.events()[4].name == "libMath" &&
        manager.events()[5].name == "libBase";
    if (!unloaded || !protectedDependency) return std::nullopt;
    GuestModuleLifecycleDiagnostic report;
    report.modulesLoaded = 3;
    report.imports = 1;
    report.instructions = run.instructions;
    report.initializations = manager.initializations();
    report.finalizations = manager.finalizations();
    report.rax = run.rax;
    report.stackRestored = run.stackRestored;
    report.importReadOnly = ro;
    report.unloaded = unloaded;
    report.dependencyProtected = protectedDependency;
    return report;
}

} // namespace misaki
