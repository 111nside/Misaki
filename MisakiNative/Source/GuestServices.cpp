#include "../Include/GuestServices.hpp"
#include "../Include/GuestCPU.hpp"

#include <algorithm>
#include <limits>
#include <utility>

namespace misaki {

GuestServiceAction GuestServiceEnvironment::dispatch(GuestMemory &memory,
                                                       X64State &state,
                                                       std::uint32_t tid) {
    if (calls_ >= 100000) return GuestServiceAction::unsupported;
    ++calls_;
    switch (state.registers[0]) {
    case guest_service::pageSize:
        state.registers[0] = 4096;
        return GuestServiceAction::resume;
    case guest_service::threadID:
        state.registers[0] = tid;
        return GuestServiceAction::resume;
    case guest_service::yield:
        state.registers[0] = 0;
        ++yields_;
        return GuestServiceAction::yield;
    case guest_service::exit:
        exits_[tid] = state.registers[7];
        return GuestServiceAction::exit;
    case guest_service::write: {
        const std::uint64_t start = state.registers[7];
        const std::uint64_t size = state.registers[6];
        if (size > 4096 || output_.size() > 16384 - static_cast<std::size_t>(size) ||
            (size != 0 && start > std::numeric_limits<std::uint64_t>::max() - (size - 1)))
            return GuestServiceAction::memoryFault;
        std::string chunk;
        chunk.reserve(static_cast<std::size_t>(size));
        for (std::uint64_t i = 0; i < size; ++i) {
            const auto byte = memory.read8(start + i);
            if (!byte) return GuestServiceAction::memoryFault;
            chunk.push_back(static_cast<char>(*byte));
        }
        // A bad pointer cannot produce a partial guest write.
        output_ += chunk;
        ++writes_;
        state.registers[0] = size;
        return GuestServiceAction::resume;
    }
    default: return GuestServiceAction::unsupported;
    }
}

bool GuestRoundRobinScheduler::addThread(GuestMemory memory, std::uint64_t entry) {
    if (hasRun_ || threads_.size() >= 8 || !memory.isExecutable(entry)) return false;
    const auto tid = static_cast<std::uint32_t>(threads_.size() + 1);
    threads_.push_back(std::make_unique<PortableX64Backend>(std::move(memory), entry,
                                                             &services_, tid));
    return true;
}

GuestScheduleReport GuestRoundRobinScheduler::run(std::uint32_t quantum,
                                                   std::uint32_t maxTotalInstructions) {
    GuestScheduleReport report;
    if (hasRun_ || threads_.empty() || quantum == 0 || quantum > 1000 ||
        maxTotalInstructions == 0 || maxTotalInstructions > 100000) return report;
    hasRun_ = true;
    report.threadResults.resize(threads_.size());
    std::vector<bool> completed(threads_.size(), false);
    std::size_t remaining = threads_.size();
    while (remaining && report.instructions < maxTotalInstructions) {
        bool advanced = false;
        for (std::size_t i = 0; i < threads_.size() && remaining; ++i) {
            if (completed[i] || report.instructions >= maxTotalInstructions) continue;
            const auto allowance = std::min(quantum, maxTotalInstructions - report.instructions);
            if (report.dispatchOrder.size() < 256)
                report.dispatchOrder.push_back(static_cast<std::uint32_t>(i + 1));
            const auto before = report.threadResults[i].state.instructions;
            auto result = threads_[i]->run(allowance);
            report.threadResults[i] = result;
            if (result.state.instructions < before) return report;
            const auto delta = result.state.instructions - before;
            report.instructions += delta;
            advanced = advanced || delta != 0;
            if (result.stop == X64Stop::yielded) ++report.yieldEvents;
            if (result.stop == X64Stop::halted || result.stop == X64Stop::exited) {
                completed[i] = true;
                --remaining;
            } else if (result.stop != X64Stop::stepLimit && result.stop != X64Stop::yielded) {
                return report; // guest fault: do not keep running remaining contexts
            }
        }
        if (!advanced) return report;
    }
    report.completed = remaining == 0;
    return report;
}

namespace {
void put(std::vector<std::uint8_t> &elf, std::size_t at,
         std::uint64_t value, unsigned count) {
    for (unsigned i = 0; i < count; ++i)
        elf[at + i] = static_cast<std::uint8_t>(value >> (8 * i));
}
std::vector<std::uint8_t> makeServicesELF() {
    // The program writes "OK", obtains a thread ID and guest page size,
    // yields once, then returns 4096 + thread ID from its own stack.
    const std::vector<std::uint8_t> code = {
        0x48,0xb8,0x01,0x01,0,0,0,0,0,0,        // mov rax, 0x101 (write)
        0x48,0xbf,0,0x40,0,0,0,0,0,0,          // mov rdi, 0x4000
        0x48,0xbe,2,0,0,0,0,0,0,0,             // mov rsi, 2
        0x0f,0x05,                              // syscall
        0x48,0xb8,0x04,0x01,0,0,0,0,0,0,       // mov rax, 0x104 (thread ID)
        0x0f,0x05,                              // syscall
        0x48,0x89,0xc3,                         // mov rbx, rax
        0x48,0xb8,0,0x01,0,0,0,0,0,0,          // mov rax, 0x100 (page size)
        0x0f,0x05,                              // syscall
        0x48,0x01,0xd8,                         // add rax, rbx
        0x50,                                   // push rax
        0x48,0xb8,0x02,0x01,0,0,0,0,0,0,       // mov rax, 0x102 (yield)
        0x0f,0x05,                              // syscall
        0x58,                                   // pop rax
        0xf4                                    // hlt
    };
    std::vector<std::uint8_t> elf(0x302, 0);
    elf[0] = 0x7f; elf[1] = 'E'; elf[2] = 'L'; elf[3] = 'F';
    elf[4] = 2; elf[5] = 1; elf[6] = 1;
    put(elf, 16, 2, 2); put(elf, 18, 0x3e, 2); put(elf, 20, 1, 4);
    put(elf, 24, 0x2000, 8); put(elf, 32, 64, 8);
    put(elf, 52, 64, 2); put(elf, 54, 56, 2); put(elf, 56, 2, 2);
    put(elf, 64, 1, 4); put(elf, 68, 5, 4);  // RX code
    put(elf, 72, 0x100, 8); put(elf, 80, 0x2000, 8);
    put(elf, 96, code.size(), 8); put(elf, 104, code.size(), 8);
    put(elf, 120, 1, 4); put(elf, 124, 6, 4); // RW message
    put(elf, 128, 0x300, 8); put(elf, 136, 0x4000, 8);
    put(elf, 152, 2, 8); put(elf, 160, 2, 8);
    std::copy(code.begin(), code.end(), elf.begin() + 0x100);
    elf[0x300] = 'O'; elf[0x301] = 'K';
    return elf;
}
} // namespace

std::optional<GuestServiceDiagnostic> runGuestServiceDiagnostic() {
    const auto elf = makeServicesELF();
    GuestRoundRobinScheduler scheduler;
    for (int i = 0; i < 2; ++i) {
        GuestMemory memory;
        const auto image = loadGuestELF(elf.data(), elf.size(), 0, memory);
        if (!image || image->segments != 2 || !scheduler.addThread(std::move(memory), image->entry))
            return std::nullopt;
    }
    GuestServiceDiagnostic diagnostic;
    diagnostic.scheduler = scheduler.run(4, 100);
    diagnostic.output = scheduler.services().output();
    diagnostic.calls = scheduler.services().serviceCalls();
    diagnostic.writes = scheduler.services().writes();
    return diagnostic;
}

} // namespace misaki
