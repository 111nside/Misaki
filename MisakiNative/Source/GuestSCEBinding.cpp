#include "../Include/GuestSCEBinding.hpp"
#include <set>
#include <utility>
namespace misaki {
std::optional<SCEBoundSlots> bindSCEImportSlots(
    const PS4ImportCatalog &catalog, const std::vector<SCEImportSlot> &slots,
    std::uint64_t start, std::size_t length, GuestMemory &memory,
    std::string *error) {
    if (error) error->clear();
    const auto fail = [&](const char *msg) -> std::optional<SCEBoundSlots> {
        if (error) *error = msg;
        return std::nullopt;
    };
    if (slots.empty() || slots.size() > 64 || length < 8 || length > 4096 ||
        start > UINT64_MAX - length) return fail("Invalid import region or budget");
    std::set<std::uint64_t> occupied;
    std::vector<std::uint64_t> targets;
    targets.reserve(slots.size());
    for (const auto &slot : slots) {
        if (slot.address < start || slot.address - start > length-8 ||
            (slot.address & 7u) != 0 || !occupied.insert(slot.address).second)
            return fail("Import slot is unaligned, duplicated or outside binding region");
        const auto address = catalog.resolve(slot.symbol);
        if (!address || !memory.isExecutable(*address))
            return fail("Unresolved or non-executable SCE-shaped synthetic import");
        targets.push_back(*address);
    }
    GuestMemory scratch = memory;
    for (std::size_t i=0; i<slots.size(); ++i)
        if (!scratch.write64(slots[i].address, targets[i]))
            return fail("Guest import slot is not writable");
    if (!scratch.protect(start, length, permission::read))
        return fail("Guest import region must be an independently mapped region");
    for (std::size_t i=0; i<slots.size(); ++i)
        if (scratch.read64(slots[i].address) != targets[i] ||
            scratch.write64(slots[i].address, targets[i]))
            return fail("Import binding seal failed");
    memory = std::move(scratch);
    return SCEBoundSlots{static_cast<std::uint32_t>(targets.size()),
                         targets.front(),true};
}
} // namespace misaki
