#pragma once
#include "GuestVM48.hpp"
#include <cstddef>
#include <cstdint>
#include <map>
#include <optional>
namespace misaki {
// Private, deterministic process/memory-handle layer (NOT PS4 kernel calls).
// Each handle has an independent page-granular virtual address space.
class GuestKernel53 {
public:
    std::optional<std::uint32_t> createProcess();
    std::optional<std::uint32_t> cloneProcess(std::uint32_t source);
    bool terminate(std::uint32_t handle);
    bool map(std::uint32_t handle,std::uint64_t address,std::size_t bytes,std::uint8_t flags);
    bool protect(std::uint32_t handle,std::uint64_t address,std::size_t bytes,std::uint8_t flags);
    bool unmap(std::uint32_t handle,std::uint64_t address,std::size_t bytes);
    bool write(std::uint32_t handle,std::uint64_t address,const std::uint8_t *bytes,std::size_t count);
    std::optional<std::uint8_t> read(std::uint32_t handle,std::uint64_t address) const;
    std::size_t processCount() const {return processes_.size();}
    std::size_t pages(std::uint32_t handle) const;
private:
    std::map<std::uint32_t,GuestVM48> processes_;
    std::uint32_t nextHandle_=100;
};
struct GuestKernel53Report {
    std::uint32_t parent=0,child=0;
    std::uint32_t activeProcesses=0, mappedPages=0;
    bool isolated=false, sealedReadOnly=false, terminated=false;
};
std::optional<GuestKernel53Report> runGuestKernel53Diagnostic();
}
