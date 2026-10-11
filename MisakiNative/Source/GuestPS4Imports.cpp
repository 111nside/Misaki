#include "../Include/GuestPS4Imports.hpp"
#include <tuple>
namespace misaki {
namespace {
bool validModule(const std::string &s) {
    if (s.size() < 7 || s.size() > 64 || s.compare(0,6,"libSce") != 0) return false;
    for (const auto c : s)
        if (!((c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') ||
            (c >= '0' && c <= '9') || c=='_')) return false;
    return true;
}
bool validNID(const std::string &s) {
    if (s.size()!=11) return false;
    for (const auto c : s)
        if (!((c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') ||
              (c >= '0' && c <= '9') || c=='+' || c=='-' || c=='_')) return false;
    return true;
}
}
bool PS4ImportKey::operator<(const PS4ImportKey &o) const {
    return std::tie(module,library,nid,major,minor) <
           std::tie(o.module,o.library,o.nid,o.major,o.minor);
}
bool PS4ImportCatalog::define(const GuestMemory &memory, PS4ImportBinding binding) {
    const auto &k = binding.key;
    if (!validModule(k.module) || !validModule(k.library) ||
        !validNID(k.nid) || k.major == 0 || binding.guestAddress == 0 ||
        !memory.isExecutable(binding.guestAddress) ||
        entries_.size() >= 256) return false;
    return entries_.emplace(std::move(binding.key), binding.guestAddress).second;
}
std::optional<std::uint64_t> PS4ImportCatalog::resolve(const PS4ImportKey &key) const {
    const auto it = entries_.find(key);
    if (it==entries_.end()) return std::nullopt;
    return it->second;
}
std::optional<std::vector<std::uint64_t>> PS4ImportCatalog::resolveAll(
        const std::vector<PS4ImportKey> &requests) const {
    if (requests.empty() || requests.size() > 256) return std::nullopt;
    std::vector<std::uint64_t> out;
    for (const auto &r : requests) {
        const auto value=resolve(r);
        if (!value) return std::nullopt;
        out.push_back(*value);
    }
    return out;
}
PS4ImportKey makePS4SyntheticKey(std::uint32_t which) {
    // Generated fixture labels, NOT real-world NIDs or real Sony function mappings.
    return PS4ImportKey{"libSceTestModule", "libSceTestLibrary",
                        which == 0 ? "TestNid0001" : "TestNid0002", 1, 0};
}
bool installPS4SyntheticImportFunctions(GuestMemory &memory) {
    // Two independently written toy functions, each MOV RAX,imm32-like + HLT;
    // the address validation intentionally does not inspect their semantics.
    const std::uint8_t bytes[0x20] = {0x48, 0xB8, 42,0,0,0,0,0,0,0, 0xF4,
        0,0,0,0,0, 0x48, 0xB8, 7,0,0,0,0,0,0,0, 0xF4};
    return memory.map(0x9000, sizeof(bytes), permission::read | permission::write) &&
           memory.writeBytes(0x9000, bytes, sizeof(bytes)) &&
           memory.protect(0x9000, sizeof(bytes), permission::read | permission::execute);
}
std::optional<PS4ImportCatalog> makePS4SyntheticImportCatalog(const GuestMemory &memory) {
    PS4ImportCatalog catalog;
    if (!catalog.define(memory, {makePS4SyntheticKey(0),0x9000}) ||
        !catalog.define(memory, {makePS4SyntheticKey(1),0x9010})) return std::nullopt;
    return catalog;
}
}
