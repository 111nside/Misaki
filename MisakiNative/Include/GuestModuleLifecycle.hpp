#pragma once

#include "MisakiCore.hpp"

#include <cstddef>
#include <cstdint>
#include <map>
#include <optional>
#include <set>
#include <string>
#include <vector>

namespace misaki {

// Independently authored guest modules only. No Sony PRX, SELF or SCE loaders.
// Modules own small immutable RX images in a dedicated GuestMemory instance.
struct GuestModuleDefinition {
    std::string name;
    std::uint64_t address = 0;
    std::vector<std::uint8_t> image;
    std::vector<std::string> needed;
    std::map<std::string, std::uint64_t> exports;
};

struct GuestModuleEvent {
    enum class Kind : std::uint8_t { initialized, finalized };
    Kind kind;
    std::string name;
};

class GuestModuleLifecycle {
public:
    // Definitions are registered without loading. Forward dependencies allowed.
    bool define(GuestModuleDefinition module);
    // Every open increments an explicit root reference. Dependencies are retained
    // once for each loaded parent, not once per repeated open of that parent.
    bool acquire(const std::string &name);
    bool release(const std::string &name);
    bool loaded(const std::string &name) const;
    std::uint32_t references(const std::string &name) const;
    std::uint32_t opens(const std::string &name) const;
    std::size_t loadedCount() const { return active_.size(); }
    std::size_t definitionCount() const { return definitions_.size(); }
    std::uint32_t initializations() const { return initializations_; }
    std::uint32_t finalizations() const { return finalizations_; }
    const std::vector<GuestModuleEvent> &events() const { return events_; }
    const GuestMemory &memory() const { return memory_; }
    // Active libraries only; addresses come from validated RX guest mappings.
    std::optional<ModuleRegistry> activeRegistry() const;
private:
    std::map<std::string, GuestModuleDefinition> definitions_;
    std::map<std::string, std::uint32_t> roots_;
    std::set<std::string> active_;
    std::map<std::string, std::uint32_t> references_;
    GuestMemory memory_;
    std::uint32_t initializations_ = 0;
    std::uint32_t finalizations_ = 0;
    std::vector<GuestModuleEvent> events_;

    bool transition(const std::map<std::string, std::uint32_t> &nextRoots);
    static bool validName(const std::string &name);
};

struct GuestModuleLifecycleDiagnostic {
    std::uint32_t modulesLoaded = 0;
    std::uint32_t imports = 0;
    std::uint32_t instructions = 0;
    std::uint32_t initializations = 0;
    std::uint32_t finalizations = 0;
    std::uint64_t rax = 0;
    bool stackRestored = false;
    bool importReadOnly = false;
    bool unloaded = false;
    bool dependencyProtected = false;
};

std::optional<GuestModuleLifecycleDiagnostic> runGuestModuleLifecycleDiagnostic();

} // namespace misaki
