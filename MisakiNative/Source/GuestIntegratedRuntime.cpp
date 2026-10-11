#include "../Include/GuestIntegratedRuntime.hpp"

#include <algorithm>
#include <array>
#include <limits>
#include <utility>

namespace misaki {
namespace {
constexpr std::uint64_t guestStart = 0x2000;
constexpr std::uint64_t pathAddress = 0x7000;
constexpr std::uint64_t readAddress = 0x7100;
constexpr std::uint64_t shaderAddress = 0x100000;
constexpr std::uint64_t inputDescriptor = 0x110000;
constexpr std::uint64_t outputDescriptor = 0x110010;
constexpr std::uint64_t inputAddress = 0x400000;
constexpr std::uint64_t outputAddress = 0x500000;

void u64(std::vector<std::uint8_t> &bytes, std::uint64_t value) {
    for (unsigned k = 0; k < 8; ++k)
        bytes.push_back(static_cast<std::uint8_t>(value >> (k * 8)));
}
void mov(std::vector<std::uint8_t> &code, unsigned reg, std::uint64_t immediate) {
    code.push_back(0x48);
    code.push_back(static_cast<std::uint8_t>(0xB8 + reg));
    u64(code, immediate);
}
void call(std::vector<std::uint8_t> &code, std::uint64_t service) {
    mov(code, 0, service);
    code.push_back(0x0f); code.push_back(0x05);
}
std::uint64_t pair(std::uint32_t a, std::uint32_t b) {
    return std::uint64_t(a) | (std::uint64_t(b) << 32);
}
void command(std::vector<std::uint8_t> &bytes, G21Opcode opcode,
             std::uint32_t x = 0, std::uint32_t y = 0,
             std::uint32_t width = 0, std::uint32_t height = 0,
             std::uint32_t data = 0, std::uint32_t extra = 0) {
    u64(bytes, static_cast<std::uint32_t>(opcode));
    u64(bytes, pair(x, y));
    u64(bytes, pair(width, height));
    u64(bytes, pair(data, extra));
}
std::vector<std::uint8_t> wordsAsBytes(const std::uint32_t *words, std::size_t count) {
    std::vector<std::uint8_t> bytes;
    bytes.reserve(count * 4);
    for (std::size_t i = 0; i < count; ++i)
        for (unsigned shift = 0; shift < 32; shift += 8)
            bytes.push_back(static_cast<std::uint8_t>(words[i] >> shift));
    return bytes;
}
std::optional<std::uint32_t> read32(const GuestMemory &memory, std::uint64_t address) {
    if (address > UINT64_MAX - 3) return std::nullopt;
    std::uint32_t out = 0;
    for (unsigned k = 0; k < 4; ++k) {
        const auto byte = memory.read8(address + k);
        if (!byte) return std::nullopt;
        out |= std::uint32_t(*byte) << (8 * k);
    }
    return out;
}
}

IntegratedProcessServices::IntegratedProcessServices(
    const GuestVirtualFileSystem &fs, std::uint32_t pid)
    : pid_(pid), files_(fs, pid) {}

GuestServiceAction IntegratedProcessServices::dispatch(
    GuestMemory &memory, X64State &state, std::uint32_t threadID) {
    const auto operation = state.registers[0];
    // GPU/GCN calls use a distinct, original test ABI. Invalid guest requests
    // return UINT64_MAX without ever invoking a host GPU or host filesystem.
    if (operation >= runtime_service::gpuSubmit && operation <= runtime_service::pm4Inspect) {
        const auto reject = [&]() {
            ++rejectedCalls_;
            state.registers[0] = runtime_service::failure;
            return GuestServiceAction::resume;
        };
        if (operation == runtime_service::gpuSubmit) {
            const auto count = state.registers[6], fence = state.registers[2];
            // Sequential fences are required for this bounded toy scheduler.
            if (count > g21MaximumCommands || fence == 0 ||
                fence != gpu_.lastSubmittedFence() + 1 ||
                !gpu_.submit(memory, state.registers[7],
                             static_cast<std::uint32_t>(count), fence)) return reject();
            ++gpuCalls_;
            state.registers[0] = fence;
            return GuestServiceAction::resume;
        }
        if (operation == runtime_service::gpuPoll) {
            state.registers[0] = gpu_.completedFence();
            return GuestServiceAction::resume;
        }
        if (operation == runtime_service::gpuWait) {
            const auto requested = state.registers[7];
            if (requested == 0 || requested > gpu_.lastSubmittedFence()) return reject();
            while (gpu_.completedFence() < requested) {
                auto submission = gpu_.take();
                if (!submission) return reject();
                lastFrame_ = std::move(submission);
            }
            state.registers[0] = gpu_.completedFence();
            return GuestServiceAction::resume;
        }
        if (operation == runtime_service::pm4Inspect) {
            const auto words = state.registers[6];
            if (words == 0 || words > 4096) return reject();
            const auto trace = decodePM4Indirect(memory, state.registers[7],
                                                  static_cast<std::size_t>(words));
            if (!trace) return reject();
            lastPM4_ = *trace;
            ++pm4Calls_;
            state.registers[0] = trace->packets.size();
            return GuestServiceAction::resume;
        }
        if (operation == runtime_service::shaderRun) {
            const auto words = state.registers[6];
            if (words == 0 || words > 64) return reject();
            const auto program = decodeGuestFusedGCN30(memory, state.registers[7],
                                                       static_cast<std::size_t>(words));
            if (!program) return reject();
            std::array<std::uint32_t, 64> output{};
            for (std::uint32_t i = 0; i < 64; ++i) {
                const auto value = read32(memory, outputAddress + std::uint64_t(i) * 4);
                if (!value) return reject();
                output[i] = *value;
            }
            // The evaluator checks descriptor addresses, permissions, record
            // bounds, and supported instruction modes before committing writes.
            const auto execution = executeFusedGCN30(
                *program, memory, state.registers[2], state.registers[1], output);
            if (!execution) return reject();
            lastShader_ = *execution;
            ++shaderCalls_;
            state.registers[0] = execution->stored;
            return GuestServiceAction::resume;
        }
    }
    // Process-specific descriptor table and immutable guest virtual files.
    const bool reading = operation == process_service::read;
    const auto address = state.registers[6];
    const auto length = state.registers[2];
    const auto action = files_.dispatch(memory, state, threadID);
    if (reading && action == GuestServiceAction::resume &&
        state.registers[0] != process_service::failure) {
        const auto count = state.registers[0];
        if (count <= length && count <= 4096 && address <= UINT64_MAX - count) {
            std::vector<std::uint8_t> collected;
            collected.reserve(static_cast<std::size_t>(count));
            for (std::uint64_t i = 0; i < count; ++i) {
                const auto value = memory.read8(address + i);
                if (!value) { collected.clear(); break; }
                collected.push_back(*value);
            }
            lastFileRead_ = std::move(collected);
        }
    }
    return action;
}

struct IntegratedGuestRuntime::Slot {
    std::uint32_t pid = 0;
    std::unique_ptr<IntegratedProcessServices> services;
    std::unique_ptr<PortableX64Backend> cpu;
    std::unique_ptr<GuestModuleLifecycle> modules;
    std::uint32_t imported = 0;
};

IntegratedGuestRuntime::IntegratedGuestRuntime() = default;
IntegratedGuestRuntime::~IntegratedGuestRuntime() = default;

bool IntegratedGuestRuntime::addFile(const std::string &path,
                                     std::vector<std::uint8_t> contents) {
    return !ran_ && slots_.empty() && fs_.addFile(path, std::move(contents));
}
bool IntegratedGuestRuntime::addPrepared(
    GuestMemory memory, std::uint64_t entry,
    std::unique_ptr<GuestModuleLifecycle> modules, std::uint32_t imports) {
    if (ran_ || slots_.size() >= 4 || !memory.isExecutable(entry)) return false;
    auto slot = std::make_unique<Slot>();
    slot->pid = static_cast<std::uint32_t>(1001 + slots_.size());
    slot->services = std::make_unique<IntegratedProcessServices>(fs_, slot->pid);
    slot->cpu = std::make_unique<PortableX64Backend>(std::move(memory), entry,
                                                     slot->services.get(), slot->pid);
    slot->modules = std::move(modules);
    slot->imported = imports;
    slots_.push_back(std::move(slot));
    return true;
}
bool IntegratedGuestRuntime::addProcess(GuestMemory memory, std::uint64_t entry) {
    return addPrepared(std::move(memory), entry, nullptr, 0);
}
bool IntegratedGuestRuntime::addDynamicProcess(
    const std::vector<std::uint8_t> &elf,
    const std::vector<GuestModuleDefinition> &definitions,
    const std::string &root, std::uint64_t loadBias) {
    if (ran_ || slots_.size() >= 4 || elf.empty() || definitions.empty()) return false;
    auto modules = std::make_unique<GuestModuleLifecycle>();
    for (const auto &definition : definitions)
        if (!modules->define(definition)) return false;
    if (!modules->acquire(root)) return false;
    const auto registry = modules->activeRegistry();
    if (!registry) return false;
    GuestMemory memory = modules->memory();
    const auto linked = loadAndLinkDynamicELF(elf.data(), elf.size(),
                                               loadBias, *registry, memory);
    if (!linked || linked->importedSymbols == 0 ||
        linked->neededLibraries == 0) return false;
    return addPrepared(std::move(memory), linked->entry, std::move(modules),
                       linked->importedSymbols);
}
const IntegratedProcessServices *IntegratedGuestRuntime::services(std::size_t index) const {
    return index < slots_.size() ? slots_[index]->services.get() : nullptr;
}

IntegratedRuntimeResult IntegratedGuestRuntime::run(std::uint32_t quantum,
                                                    std::uint32_t maximum) {
    IntegratedRuntimeResult report;
    if (ran_ || slots_.empty() || quantum == 0 || quantum > 1000 ||
        maximum == 0 || maximum > 100000) return report;
    ran_ = true;
    const auto count = slots_.size();
    report.processes.resize(count);
    std::vector<bool> finished(count, false);
    std::size_t remaining = count;
    bool healthy = true;
    while (remaining && report.instructions < maximum && healthy) {
        bool madeProgress = false;
        for (std::size_t i = 0; i < count && remaining && report.instructions < maximum; ++i) {
            if (finished[i]) continue;
            auto &p = report.processes[i];
            auto &slot = *slots_[i];
            const auto allowance = std::min(quantum, maximum - report.instructions);
            const auto before = p.execution.state.instructions;
            p.execution = slot.cpu->run(allowance);
            const auto after = p.execution.state.instructions;
            if (after < before) { healthy = false; break; }
            report.instructions += after - before;
            madeProgress = madeProgress || after > before;
            ++report.contextSwitches;
            if (p.execution.stop == X64Stop::halted || p.execution.stop == X64Stop::exited) {
                finished[i] = true;
                --remaining;
            } else if (p.execution.stop != X64Stop::yielded &&
                       p.execution.stop != X64Stop::stepLimit) {
                healthy = false;
                break;
            }
        }
        if (!madeProgress) break;
    }
    report.completed = remaining == 0 && healthy;
    for (std::size_t i = 0; i < count; ++i) {
        auto &p = report.processes[i];
        const auto &slot = *slots_[i];
        const auto &s = *slot.services;
        p.pid = slot.pid;
        p.opens = s.files().opens(); p.reads = s.files().reads();
        p.closes = s.files().closes();
        p.gpuCalls = s.gpuCalls(); p.shaderCalls = s.shaderCalls();
        p.pm4Calls = s.pm4Calls(); p.rejectedCalls = s.rejectedCalls();
        p.submittedFence = s.submittedFence();
        p.completedFence = s.completedFence();
        p.pendingGPU = static_cast<std::uint32_t>(s.pendingGPU());
        p.loadedModules = slot.modules ? static_cast<std::uint32_t>(slot.modules->loadedCount()) : 0;
        p.resolvedImports = slot.imported;
        p.lastRead = s.lastFileRead();
        if (const auto frame = s.lastFrame()) {
            p.framebufferChecksum = frame->surface.checksum;
            p.framebufferSprites = frame->surface.sprites;
            p.framebuffer = frame->surface.pixels;
        }
        if (const auto shader = s.lastShader()) {
            p.shaderExecuted = shader->executed;
            p.shaderLoaded = shader->loaded;
            p.shaderStored = shader->stored;
        }
        if (const auto pm4 = s.lastPM4())
            p.pm4Packets = static_cast<std::uint32_t>(pm4->packets.size());
    }
    return report;
}

std::vector<std::uint8_t> makeRuntimeG21Commands() {
    std::vector<std::uint8_t> bytes;
    command(bytes, G21Opcode::clear, 0, 0, 0, 0, 0x0C1426FFu);
    command(bytes, G21Opcode::fill, 16, 18, 288, 144, 0x263A61FFu);
    command(bytes, G21Opcode::scissor, 24, 28, 272, 124);
    command(bytes, G21Opcode::state, 0, 0, 0, 0, 0xFFFFFFFFu);
    command(bytes, G21Opcode::sprite, 32, 62, 48, 48, 1);
    command(bytes, G21Opcode::state, 1, 0, 0, 0, 0xDAE9FFFFu);
    command(bytes, G21Opcode::sprite, 208, 78, 56, 56, 2);
    command(bytes, G21Opcode::scissor, 0, 0, 320, 180);
    command(bytes, G21Opcode::fill, 14, 168, 292, 8, 0x354E7FFFu);
    command(bytes, G21Opcode::present);
    return bytes;
}
std::vector<std::uint8_t> makeRuntime31Program(std::uint32_t tag) {
    std::vector<std::uint8_t> code;
    mov(code, 3, tag);  // RBX: process-private marker
    call(code, process_service::processID);
    code.push_back(0xf4);
    return code;
}
std::vector<std::uint8_t> makeRuntime35Program() {
    std::vector<std::uint8_t> code;
    mov(code, 7, pathAddress);
    mov(code, 6, 0);
    call(code, process_service::open);
    // The isolated fixture VFS assigns fd=3 to the first open.
    mov(code, 7, 3); mov(code, 6, readAddress); mov(code, 2, 5);
    call(code, process_service::read);
    mov(code, 7, 3);
    call(code, process_service::close);
    // Guest CPU patches sprite X in guest-owned GPU command memory.
    mov(code, 7, g21QueueAddress + 4 * g21RecordBytes + 8);
    mov(code, 0, pair(44, 62));
    code.insert(code.end(), {0x48, 0x89, 0x07}); // MOV [RDI],RAX
    mov(code, 7, g21QueueAddress); mov(code, 6, 10); mov(code, 2, 1);
    call(code, runtime_service::gpuSubmit);
    mov(code, 7, 1);
    call(code, runtime_service::gpuWait);
    const auto pm4 = makePM4IndirectFixture();
    mov(code, 7, pm4.root);
    mov(code, 6, pm4.rootWords.size());
    call(code, runtime_service::pm4Inspect);
    mov(code, 7, shaderAddress); mov(code, 6, makeFusedGCN30Fixture().size());
    mov(code, 2, inputDescriptor); mov(code, 1, outputDescriptor);
    call(code, runtime_service::shaderRun);
    call(code, process_service::processID);
    code.push_back(0xf4);
    return code;
}

bool installRuntimeGCN(GuestMemory &memory) {
    const auto code = makeFusedGCN30Fixture();
    const auto codeBytes = wordsAsBytes(code.data(), code.size());
    auto input = makeGCNBufferFixture();
    auto output = input;
    output[0] = static_cast<std::uint32_t>(outputAddress);
    output[1] = 4u << 16;
    const auto sourceDescriptor = wordsAsBytes(input.data(), input.size());
    const auto targetDescriptor = wordsAsBytes(output.data(), output.size());
    std::array<std::uint8_t, 1024> inputBytes{};
    for (unsigned i = 0; i < 64; ++i)
        for (unsigned k = 0; k < 4; ++k)
            inputBytes[i * 16 + k] = static_cast<std::uint8_t>((i * 3 + 10) >> (k * 8));
    std::array<std::uint32_t, 64> initial{};
    for (unsigned i = 0; i < 64; ++i) initial[i] = 0xB0000000u + i;
    const auto outputBytes = wordsAsBytes(initial.data(), initial.size());
    return memory.map(shaderAddress, codeBytes.size(), permission::read | permission::write) &&
           memory.writeBytes(shaderAddress, codeBytes.data(), codeBytes.size()) &&
           memory.protect(shaderAddress, codeBytes.size(), permission::read) &&
           memory.map(inputDescriptor, 32, permission::read | permission::write) &&
           memory.writeBytes(inputDescriptor, sourceDescriptor.data(), sourceDescriptor.size()) &&
           memory.writeBytes(outputDescriptor, targetDescriptor.data(), targetDescriptor.size()) &&
           memory.protect(inputDescriptor, 32, permission::read) &&
           memory.map(inputAddress, inputBytes.size(), permission::read | permission::write) &&
           memory.writeBytes(inputAddress, inputBytes.data(), inputBytes.size()) &&
           memory.protect(inputAddress, inputBytes.size(), permission::read) &&
           memory.map(outputAddress, outputBytes.size(), permission::read | permission::write) &&
           memory.writeBytes(outputAddress, outputBytes.data(), outputBytes.size());
}
bool installRuntimePM4(GuestMemory &memory) {
    return installPM4IndirectFixture(memory, makePM4IndirectFixture());
}
bool installRuntime35Memory(GuestMemory &memory) {
    const auto code = makeRuntime35Program();
    const auto commands = makeRuntimeG21Commands();
    const std::vector<std::uint8_t> path = {
        '/', 'd', 'a', 't', 'a', '/', 'b', 'o', 'o', 't', '.', 't', 'x', 't', 0
    };
    return memory.map(guestStart, code.size(), permission::read | permission::write) &&
           memory.writeBytes(guestStart, code.data(), code.size()) &&
           memory.protect(guestStart, code.size(), permission::read | permission::execute) &&
           memory.map(g21QueueAddress, commands.size(), permission::read | permission::write) &&
           memory.writeBytes(g21QueueAddress, commands.data(), commands.size()) &&
           memory.map(pathAddress, path.size(), permission::read | permission::write) &&
           memory.writeBytes(pathAddress, path.data(), path.size()) &&
           memory.protect(pathAddress, path.size(), permission::read) &&
           memory.map(readAddress, 32, permission::read | permission::write) &&
           installRuntimeGCN(memory) && installRuntimePM4(memory);
}
std::vector<GuestModuleDefinition> makeRuntime34Modules() {
    return {
        {"libBase", 0xA000, {0x90,0xC3}, {}, {}},
        {"libMisakiDynamic", 0x9000, {0x48,0x83,0xC0,0x02,0xC3},
         {"libBase"}, {{"increment2", 0x9000}}}
    };
}

std::optional<IntegratedMegaDiagnostic> runIntegratedMegaDiagnostic() {
    IntegratedGuestRuntime runtime;
    if (!runtime.addFile("/data/boot.txt", {'H','e','l','l','o'})) return std::nullopt;
    GuestMemory memory;
    if (!installRuntime35Memory(memory) || !runtime.addProcess(std::move(memory), guestStart) ||
        !runtime.addDynamicProcess(makeDynamicCPUFixture(), makeRuntime34Modules(),
                                   "libMisakiDynamic", 0x4000)) return std::nullopt;
    IntegratedMegaDiagnostic report;
    report.runtime = runtime.run(4, 512);
    if (report.runtime.processes.size() != 2) return std::nullopt;
    const auto &app = report.runtime.processes[0];
    const auto &elf = report.runtime.processes[1];
    report.stage31 = report.runtime.completed && report.runtime.contextSwitches > 2 &&
        app.pid != elf.pid && app.execution.stop == X64Stop::halted &&
        elf.execution.stop == X64Stop::halted && app.execution.stackRestored() &&
        elf.execution.stackRestored();
    report.stage32 = app.gpuCalls == 1 && app.submittedFence == 1 &&
        app.completedFence == 1 && app.pendingGPU == 0 &&
        app.framebufferChecksum != 0 && app.framebufferSprites == 2 &&
        app.framebuffer.size() == 320 * 180 * 4;
    report.stage33 = app.pm4Calls == 1 && app.pm4Packets == 11 &&
        app.shaderCalls == 1 && app.shaderExecuted == 11 &&
        app.shaderLoaded == 31 && app.shaderStored == 31;
    report.stage34 = elf.resolvedImports == 1 && elf.loadedModules == 2 &&
        elf.execution.rax() == 42;
    report.stage35 = app.opens == 1 && app.reads == 1 && app.closes == 1 &&
        app.lastRead == std::vector<std::uint8_t>({'H','e','l','l','o'}) &&
        app.execution.rax() == app.pid && app.rejectedCalls == 0;
    return report;
}
} // namespace misaki
