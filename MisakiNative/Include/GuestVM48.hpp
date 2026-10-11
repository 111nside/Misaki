#pragma once
#include "MisakiCore.hpp"
#include <cstddef>
#include <cstdint>
#include <optional>
#include <vector>
namespace misaki {
// Bounded page-granular guest VM manager, NOT Sony mmap/sceKernel APIs.
// Unmapping rebuilds an independent GuestMemory snapshot because GuestMemory
// deliberately has no unmap primitive. Public access is read-only.
class GuestVM48 {
public:
    bool map(std::uint64_t guestStart,std::size_t length,std::uint8_t permissionBits);
    bool protect(std::uint64_t guestStart,std::size_t length,std::uint8_t permissionBits);
    bool unmap(std::uint64_t guestStart,std::size_t length);
    bool write(std::uint64_t address,const std::uint8_t *data,std::size_t length);
    std::optional<std::uint8_t> read(std::uint64_t address) const;
    std::size_t mappedBytes() const {return mapped_;}
    std::size_t regions() const {return maps_.size();}
    const GuestMemory &snapshot() const {return memory_;}
private:
    struct Region {std::uint64_t start;std::uint8_t flags;std::vector<std::uint8_t> data;};
    std::vector<Region> maps_;
    GuestMemory memory_;
    std::size_t mapped_=0;
    static bool allowed(std::uint8_t flags);
    static bool rebuild(const std::vector<Region>&regions,GuestMemory &out);
};
}
