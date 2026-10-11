#pragma once
#include "GuestPS4Imports.hpp"
#include "GuestSCEKernel.hpp"
#include <cstddef>
#include <cstdint>
#include <map>
#include <optional>
#include <string>

namespace misaki {
// Explicit user-authored NID catalog -> private Misaki service ABI.
// There is no Sony service-name database, no host syscall dispatch, and no HLE
// compatibility implied by a matching NID shape.
enum class PrivateService57 : std::uint8_t { pid, monotonicClock, write, exit };
struct ServiceBinding57 { PS4ImportKey key; PrivateService57 action; };
class ServiceRegistry57 {
public:
    bool add(ServiceBinding57 item);
    std::optional<PrivateService57> resolve(const PS4ImportKey &key) const;
    std::size_t size() const {return entries_.size();}
private:
    std::map<PS4ImportKey,PrivateService57> entries_;
};
struct Service57Result { std::uint32_t resolved=0,calls=0,failures=0,outputBytes=0; bool processIsolated=false; };
std::optional<Service57Result> runService57Diagnostic();
} // namespace misaki
