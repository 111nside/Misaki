#pragma once

#include "GuestServices.hpp"

#include <cstdint>
#include <map>
#include <memory>
#include <optional>
#include <string>
#include <vector>

namespace misaki {

// In-memory, read-only fixture filesystem. No host paths, file handles, or
// iOS filesystem calls are available to the guest. NOT PS4's filesystem.
class GuestVirtualFileSystem {
public:
    bool addFile(const std::string &path, std::vector<std::uint8_t> contents);
    const std::vector<std::uint8_t> *findFile(const std::string &path) const;
    std::size_t fileCount() const { return files_.size(); }
    static bool validPath(const std::string &path);
private:
    std::map<std::string, std::vector<std::uint8_t>> files_;
};

// Original sandbox ABI, NOT PS4 syscalls. RAX selects operation.
// open: RDI=guest NUL-terminated absolute path, RSI=0 (read-only), RAX=fd/-1
// read: RDI=fd, RSI=guest output buffer, RDX=length <=4096, RAX=bytes/-1
// close: RDI=fd, RAX=0/-1; seek: RDI=fd, RSI=absolute offset, RAX=offset/-1
// processID: RAX=the simulated PID. Files never reach the host filesystem.
namespace process_service {
constexpr std::uint64_t open = 0x200;
constexpr std::uint64_t read = 0x201;
constexpr std::uint64_t close = 0x202;
constexpr std::uint64_t seek = 0x203;
constexpr std::uint64_t processID = 0x204;
constexpr std::uint64_t failure = UINT64_MAX;
}

class GuestProcessServices final : public IGuestServiceDispatcher {
public:
    GuestProcessServices(const GuestVirtualFileSystem &filesystem, std::uint32_t pid)
        : filesystem_(filesystem), pid_(pid) {}
    GuestServiceAction dispatch(GuestMemory &memory, X64State &state,
                                std::uint32_t threadID) override;
    std::uint32_t pid() const { return pid_; }
    std::uint32_t opens() const { return opens_; }
    std::uint32_t reads() const { return reads_; }
    std::uint32_t closes() const { return closes_; }
    std::size_t openDescriptors() const { return descriptors_.size(); }
    const std::string &output() const { return base_.output(); }
    std::uint32_t yields() const { return base_.yields(); }
    std::uint32_t writes() const { return base_.writes(); }
private:
    struct Descriptor { std::string path; std::size_t offset = 0; };
    const GuestVirtualFileSystem &filesystem_;
    const std::uint32_t pid_;
    GuestServiceEnvironment base_;
    std::map<std::uint64_t, Descriptor> descriptors_;
    std::uint32_t opens_ = 0;
    std::uint32_t reads_ = 0;
    std::uint32_t closes_ = 0;
    std::optional<std::string> readPath(GuestMemory &memory, std::uint64_t address) const;
};

struct GuestProcessReport {
    bool completed = false;
    std::uint32_t instructions = 0;
    std::uint32_t yields = 0;
    std::uint32_t opens = 0;
    std::uint32_t reads = 0;
    std::uint32_t closes = 0;
    std::uint32_t writes = 0;
    std::string output;
    std::vector<X64ExecutionResult> results;
    std::vector<std::uint32_t> pids;
    std::vector<std::uint32_t> dispatchOrder;
};

// Processes have separate guest memory, stacks, and FD tables. They share
// immutable virtual file contents, not a real PS4 process address space.
class GuestProcessManager {
public:
    explicit GuestProcessManager(GuestVirtualFileSystem filesystem)
        : filesystem_(std::move(filesystem)) {}
    GuestProcessManager(const GuestProcessManager &) = delete;
    GuestProcessManager &operator=(const GuestProcessManager &) = delete;
    bool addProcess(GuestMemory memory, std::uint64_t entry);
    GuestProcessReport run(std::uint32_t quantum = 4,
                           std::uint32_t maxTotalInstructions = 1000);
    const GuestProcessServices *process(std::size_t index) const;
    std::size_t processCount() const { return processors_.size(); }
private:
    GuestVirtualFileSystem filesystem_;
    std::vector<std::unique_ptr<GuestProcessServices>> services_;
    std::vector<std::unique_ptr<PortableX64Backend>> processors_;
    bool hasRun_ = false;
};

std::optional<GuestProcessReport> runGuestProcessDiagnostic();

} // namespace misaki
