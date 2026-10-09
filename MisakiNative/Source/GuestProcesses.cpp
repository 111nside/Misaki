#include "../Include/GuestProcesses.hpp"
#include "../Include/GuestCPU.hpp"

#include <algorithm>
#include <limits>
#include <utility>

namespace misaki {

bool GuestVirtualFileSystem::validPath(const std::string &path) {
    if (path.empty() || path.size() > 128 || path.front() != '/' || path == "/") return false;
    if (path.find('\\') != std::string::npos || path.find('\0') != std::string::npos) return false;
    // Strict canonical guest paths; no host traversal, doubled separators,
    // dot components, or trailing slash. No host pathname normalization.
    std::size_t pos = 1;
    while (pos < path.size()) {
        const auto end = path.find('/', pos);
        const auto component = path.substr(pos, end == std::string::npos ? end : end - pos);
        if (component.empty() || component == "." || component == "..") return false;
        if (end == std::string::npos) break;
        pos = end + 1;
    }
    return path.back() != '/';
}

bool GuestVirtualFileSystem::addFile(const std::string &path,
                                     std::vector<std::uint8_t> contents) {
    if (!validPath(path) || contents.size() > 1024 * 1024 || files_.size() >= 32 ||
        files_.count(path) != 0) return false;
    files_.emplace(path, std::move(contents));
    return true;
}

const std::vector<std::uint8_t> *GuestVirtualFileSystem::findFile(
    const std::string &path) const {
    const auto it = files_.find(path);
    return it == files_.end() ? nullptr : &it->second;
}

std::optional<std::string> GuestProcessServices::readPath(GuestMemory &memory,
                                                           std::uint64_t address) const {
    std::string result;
    for (std::uint64_t i = 0; i <= 128; ++i) {
        if (address > std::numeric_limits<std::uint64_t>::max() - i)
            return std::nullopt;
        auto character = memory.read8(address + i);
        if (!character) return std::nullopt;
        if (*character == 0) return result;
        result.push_back(static_cast<char>(*character));
    }
    return result; // invalid path (too long) is safely rejected by validPath.
}

GuestServiceAction GuestProcessServices::dispatch(GuestMemory &memory, X64State &state,
                                                    std::uint32_t threadID) {
    const auto op = state.registers[0];
    if (op == process_service::processID) {
        state.registers[0] = pid_;
        return GuestServiceAction::resume;
    }
    if (op == process_service::open) {
        auto path = readPath(memory, state.registers[7]);
        if (!path) return GuestServiceAction::memoryFault;
        state.registers[0] = process_service::failure;
        if (state.registers[6] != 0 || !GuestVirtualFileSystem::validPath(*path) ||
            !filesystem_.findFile(*path) || descriptors_.size() >= 16)
            return GuestServiceAction::resume;
        std::uint64_t fd = 3;
        while (descriptors_.count(fd) != 0) ++fd;
        descriptors_.emplace(fd, Descriptor{*path, 0});
        ++opens_;
        state.registers[0] = fd;
        return GuestServiceAction::resume;
    }
    if (op == process_service::read) {
        const auto fd = state.registers[7];
        const auto target = state.registers[6];
        const auto length = state.registers[2];
        auto it = descriptors_.find(fd);
        state.registers[0] = process_service::failure;
        if (it == descriptors_.end() || length > 4096) return GuestServiceAction::resume;
        const auto *bytes = filesystem_.findFile(it->second.path);
        if (!bytes) return GuestServiceAction::unsupported;
        const auto available = bytes->size() - it->second.offset;
        const auto count = std::min<std::size_t>(available, static_cast<std::size_t>(length));
        if (count) {
            // Verify complete output before any write; failed memory copy must
            // preserve guest buffer and the process-specific file offset.
            if (target > std::numeric_limits<std::uint64_t>::max() - (count - 1))
                return GuestServiceAction::memoryFault;
            std::vector<std::uint8_t> chunk(bytes->begin() + it->second.offset,
                                            bytes->begin() + it->second.offset + count);
            if (!memory.writeBytes(target, chunk.data(), chunk.size()))
                return GuestServiceAction::memoryFault;
        }
        it->second.offset += count;
        ++reads_;
        state.registers[0] = count;
        return GuestServiceAction::resume;
    }
    if (op == process_service::close) {
        state.registers[0] = descriptors_.erase(state.registers[7]) ? 0 : process_service::failure;
        if (state.registers[0] == 0) ++closes_;
        return GuestServiceAction::resume;
    }
    if (op == process_service::seek) {
        const auto it = descriptors_.find(state.registers[7]);
        const auto offset = state.registers[6];
        state.registers[0] = process_service::failure;
        if (it == descriptors_.end()) return GuestServiceAction::resume;
        const auto *file = filesystem_.findFile(it->second.path);
        if (!file || offset > file->size()) return GuestServiceAction::resume;
        it->second.offset = static_cast<std::size_t>(offset);
        state.registers[0] = offset;
        return GuestServiceAction::resume;
    }
    // Other original toy services (write/yield/pageSize/exit) remain available.
    return base_.dispatch(memory, state, threadID);
}

bool GuestProcessManager::addProcess(GuestMemory memory, std::uint64_t entry) {
    if (hasRun_ || processors_.size() >= 4 || !memory.isExecutable(entry)) return false;
    const auto pid = static_cast<std::uint32_t>(1001 + processors_.size());
    services_.push_back(std::make_unique<GuestProcessServices>(filesystem_, pid));
    processors_.push_back(std::make_unique<PortableX64Backend>(
        std::move(memory), entry, services_.back().get(), pid));
    return true;
}

const GuestProcessServices *GuestProcessManager::process(std::size_t index) const {
    return index < services_.size() ? services_[index].get() : nullptr;
}

GuestProcessReport GuestProcessManager::run(std::uint32_t quantum,
                                             std::uint32_t maxTotalInstructions) {
    GuestProcessReport report;
    if (hasRun_ || processors_.empty() || quantum == 0 || quantum > 1000 ||
        maxTotalInstructions == 0 || maxTotalInstructions > 100000) return report;
    hasRun_ = true;
    const auto n = processors_.size();
    report.results.resize(n);
    std::vector<bool> stopped(n, false);
    std::size_t remaining = n;
    while (remaining && report.instructions < maxTotalInstructions) {
        bool advanced = false;
        for (std::size_t i = 0; i < n && remaining && report.instructions < maxTotalInstructions; ++i) {
            if (stopped[i]) continue;
            if (report.dispatchOrder.size() < 256)
                report.dispatchOrder.push_back(services_[i]->pid());
            const auto allowed = std::min(quantum, maxTotalInstructions - report.instructions);
            const auto before = report.results[i].state.instructions;
            const auto result = processors_[i]->run(allowed);
            report.results[i] = result;
            if (result.state.instructions < before) return report;
            const auto executed = result.state.instructions - before;
            report.instructions += executed;
            advanced = advanced || executed > 0;
            if (result.stop == X64Stop::yielded) ++report.yields;
            if (result.stop == X64Stop::halted || result.stop == X64Stop::exited) {
                stopped[i] = true;
                --remaining;
            } else if (result.stop != X64Stop::yielded && result.stop != X64Stop::stepLimit)
                return report; // halt scheduling after a fault; no host escalation
        }
        if (!advanced) return report;
    }
    report.completed = remaining == 0;
    for (const auto &process : services_) {
        report.pids.push_back(process->pid());
        report.opens += process->opens();
        report.reads += process->reads();
        report.closes += process->closes();
        report.writes += process->writes();
        report.output += process->output();
    }
    return report;
}

namespace {
void writeLE(std::vector<std::uint8_t> &bytes, std::size_t offset,
             std::uint64_t value, unsigned width) {
    for (unsigned i = 0; i < width; ++i)
        bytes[offset + i] = static_cast<std::uint8_t>(value >> (8 * i));
}
void emitMov(std::vector<std::uint8_t> &code, std::uint8_t reg, std::uint64_t value) {
    code.push_back(0x48);
    code.push_back(static_cast<std::uint8_t>(0xb8 + reg));
    for (unsigned i = 0; i < 8; ++i)
        code.push_back(static_cast<std::uint8_t>(value >> (8 * i)));
}
std::vector<std::uint8_t> makeProcessELF() {
    std::vector<std::uint8_t> code;
    const auto mov = [&](unsigned reg, std::uint64_t value) {
        emitMov(code, static_cast<std::uint8_t>(reg), value);
    };
    const auto syscall = [&]() { code.push_back(0x0f); code.push_back(0x05); };
    mov(0, process_service::open);
    mov(7, 0x4000); // /app/greeting.txt
    mov(6, 0); // read-only
    syscall();
    code.insert(code.end(), {0x48, 0x89, 0xc3}); // mov rbx,rax (fd)
    mov(0, process_service::read);
    code.insert(code.end(), {0x48, 0x89, 0xdf}); // mov rdi,rbx
    mov(6, 0x4100); // private guest buffer
    mov(2, 5);
    syscall();
    mov(0, guest_service::write);
    mov(7, 0x4100);
    mov(6, 5);
    syscall();
    mov(0, guest_service::yield);
    syscall();
    mov(0, process_service::close);
    code.insert(code.end(), {0x48, 0x89, 0xdf}); // mov rdi,rbx
    syscall();
    mov(0, process_service::processID);
    syscall();
    code.push_back(0xf4);

    constexpr std::uint64_t codeAddress = 0x2000;
    constexpr std::uint64_t dataAddress = 0x4000;
    constexpr std::size_t codeOffset = 0x100;
    constexpr std::size_t dataOffset = 0x300;
    const std::string guestPath = "/app/greeting.txt";
    std::vector<std::uint8_t> elf(dataOffset + guestPath.size() + 1, 0);
    elf[0] = 0x7f; elf[1] = 'E'; elf[2] = 'L'; elf[3] = 'F';
    elf[4] = 2; elf[5] = 1; elf[6] = 1;
    writeLE(elf, 16, 2, 2); writeLE(elf, 18, 0x3e, 2); writeLE(elf, 20, 1, 4);
    writeLE(elf, 24, codeAddress, 8); writeLE(elf, 32, 64, 8);
    writeLE(elf, 52, 64, 2); writeLE(elf, 54, 56, 2); writeLE(elf, 56, 2, 2);
    writeLE(elf, 64, 1, 4); writeLE(elf, 68, 5, 4);
    writeLE(elf, 72, codeOffset, 8); writeLE(elf, 80, codeAddress, 8);
    writeLE(elf, 96, code.size(), 8); writeLE(elf, 104, code.size(), 8);
    writeLE(elf, 120, 1, 4); writeLE(elf, 124, 6, 4);
    writeLE(elf, 128, dataOffset, 8); writeLE(elf, 136, dataAddress, 8);
    writeLE(elf, 152, guestPath.size() + 1, 8); writeLE(elf, 160, 0x200, 8);
    std::copy(code.begin(), code.end(), elf.begin() + codeOffset);
    std::copy(guestPath.begin(), guestPath.end(), elf.begin() + dataOffset);
    return elf;
}
} // namespace

std::optional<GuestProcessReport> runGuestProcessDiagnostic() {
    GuestVirtualFileSystem files;
    const std::string greeting = "Hello guest!";
    if (!files.addFile("/app/greeting.txt",
                       std::vector<std::uint8_t>(greeting.begin(), greeting.end())))
        return std::nullopt;
    auto program = makeProcessELF();
    GuestProcessManager manager(std::move(files));
    for (int i = 0; i < 2; ++i) {
        GuestMemory memory;
        auto loaded = loadGuestELF(program.data(), program.size(), 0, memory);
        if (!loaded || loaded->segments != 2 ||
            !manager.addProcess(std::move(memory), loaded->entry)) return std::nullopt;
    }
    return manager.run(4, 200);
}

} // namespace misaki
