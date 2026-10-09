#include "GuestProcesses.hpp"
#include "MisakiCoreBridge.h"

#include <cstdlib>
#include <iostream>
#include <limits>
#include <string>
#include <vector>

namespace {
int checks = 0;
void check(bool passed, const char *message) {
    ++checks;
    if (!passed) { std::cerr << "FAIL: " << message << '\n'; std::exit(EXIT_FAILURE); }
}
misaki::GuestMemory memoryWith(const std::string &path) {
    using namespace misaki;
    GuestMemory memory;
    check(memory.map(0x4000, 256, permission::read | permission::write), "guest data mapping");
    std::vector<std::uint8_t> bytes(path.begin(), path.end());
    bytes.push_back(0);
    check(memory.writeBytes(0x4000, bytes.data(), bytes.size()), "guest pathname copy");
    return memory;
}
misaki::GuestServiceAction invoke(misaki::GuestProcessServices &env,
                                 misaki::GuestMemory &memory,
                                 misaki::X64State &cpu, std::uint64_t service,
                                 std::uint64_t a = 0, std::uint64_t b = 0,
                                 std::uint64_t c = 0) {
    cpu.registers[0] = service;
    cpu.registers[7] = a;
    cpu.registers[6] = b;
    cpu.registers[2] = c;
    return env.dispatch(memory, cpu, 1);
}
misaki::GuestMemory singleInstruction(const std::vector<std::uint8_t> &program) {
    using namespace misaki;
    GuestMemory mem;
    check(mem.map(0x1000, program.size(), permission::read | permission::write), "map guest test code");
    check(mem.writeBytes(0x1000, program.data(), program.size()), "copy guest test code");
    check(mem.protect(0x1000, program.size(), permission::read | permission::execute), "protect guest test code");
    return mem;
}
}

int main() {
    using namespace misaki;
    GuestVirtualFileSystem vfs;
    const std::string text = "Hello guest!";
    check(vfs.addFile("/app/greeting.txt", {text.begin(), text.end()}), "register immutable guest file");
    check(vfs.addFile("/system/version", {'1', '.', '0'}), "register second guest file");
    check(vfs.fileCount() == 2, "guest file count");
    check(!vfs.addFile("/app/greeting.txt", {'X'}), "duplicate guest file rejected");
    check(!vfs.addFile("/app/../secrets", {'X'}), "path traversal rejected");
    check(!vfs.addFile("/app/./item", {'X'}), "dot component rejected");
    check(!vfs.addFile("/app//item", {'X'}), "empty component rejected");
    check(!vfs.addFile("/app/item/", {'X'}), "trailing slash rejected");
    check(!vfs.addFile("relative.txt", {'X'}), "relative path rejected");
    check(!vfs.addFile("/app\\escape", {'X'}), "backslash path rejected");
    check(!vfs.addFile("/", {'X'}), "directory-only path rejected");
    check(!vfs.addFile("/big", std::vector<std::uint8_t>(1024 * 1024 + 1, 0)), "oversized file rejected");
    check(!vfs.addFile("/" + std::string(130, 'a'), {'X'}), "oversized path rejected");
    check(vfs.findFile("/app/greeting.txt") && vfs.findFile("/app/greeting.txt")->size() == 12,
          "files are accessible via virtual namespace");
    check(vfs.findFile("/App/greeting.txt") == nullptr, "guest path case sensitive");

    auto memory = memoryWith("/app/greeting.txt");
    GuestProcessServices first(vfs, 1001);
    GuestProcessServices second(vfs, 1002);
    X64State a, b;
    check(invoke(first, memory, a, process_service::open, 0x4000) == GuestServiceAction::resume,
          "open read-only guest file");
    check(a.registers[0] == 3 && first.openDescriptors() == 1, "first guest fd=3");
    check(invoke(second, memory, b, process_service::open, 0x4000) == GuestServiceAction::resume,
          "other process opens same file independently");
    check(b.registers[0] == 3 && second.openDescriptors() == 1,
          "each process uses independent descriptor table");
    check(invoke(first, memory, a, process_service::open, 0x4000, 1) == GuestServiceAction::resume &&
          a.registers[0] == process_service::failure, "write-mode file access denied");
    check(invoke(first, memory, a, process_service::read, 3, 0x4080, 5) == GuestServiceAction::resume,
          "read first five bytes");
    check(a.registers[0] == 5 && memory.read8(0x4080) == 'H' && memory.read8(0x4084) == 'o',
          "guest read bytes match file");
    check(invoke(first, memory, a, process_service::read, 3, 0x4088, 5) == GuestServiceAction::resume,
          "second guest read advances offset");
    check(memory.read8(0x4088) == ' ' && memory.read8(0x408c) == 's',
          "second read continues from previous offset");
    check(invoke(second, memory, b, process_service::read, 3, 0x4090, 5) == GuestServiceAction::resume &&
          memory.read8(0x4090) == 'H', "second process starts at its own offset zero");
    check(invoke(first, memory, a, process_service::seek, 3, 0) == GuestServiceAction::resume &&
          a.registers[0] == 0, "absolute seek resets position");
    check(invoke(first, memory, a, process_service::read, 3, 0x4098, 5) == GuestServiceAction::resume &&
          memory.read8(0x4098) == 'H', "seek affects next read");
    check(invoke(first, memory, a, process_service::seek, 3, 100000) == GuestServiceAction::resume &&
          a.registers[0] == process_service::failure, "seek outside file rejected");
    check(invoke(first, memory, a, process_service::read, 3, 0x40a0, 4097) == GuestServiceAction::resume &&
          a.registers[0] == process_service::failure, "huge read rejected");
    check(invoke(first, memory, a, process_service::read, 3, std::numeric_limits<std::uint64_t>::max(), 5)
          == GuestServiceAction::memoryFault, "guest pointer overflow faults");
    check(invoke(first, memory, a, process_service::read, 3, 0x6000, 5)
          == GuestServiceAction::memoryFault, "unmapped read buffer faults");
    check(invoke(first, memory, a, process_service::read, 3, 0x40a0, 1) == GuestServiceAction::resume &&
          memory.read8(0x40a0) == ' ', "failed read preserves cursor");
    check(invoke(first, memory, a, process_service::read, 3, 0x40ff, 4)
          == GuestServiceAction::memoryFault, "cross-region copy fails safely");
    check(memory.read8(0x40ff) == 0, "failed write is atomic");
    check(invoke(first, memory, a, process_service::close, 3) == GuestServiceAction::resume &&
          a.registers[0] == 0, "close successful");
    check(invoke(first, memory, a, process_service::close, 3) == GuestServiceAction::resume &&
          a.registers[0] == process_service::failure, "double-close rejected");
    check(invoke(second, memory, b, process_service::read, 3, 0x40b0, 1) == GuestServiceAction::resume &&
          b.registers[0] == 1, "closing first process does not close second fd");
    check(invoke(first, memory, a, process_service::read, 3, 0x40b0, 1) == GuestServiceAction::resume &&
          a.registers[0] == process_service::failure, "closed fd cannot read");
    check(invoke(first, memory, a, process_service::processID) == GuestServiceAction::resume &&
          a.registers[0] == 1001, "process ID is deterministic");
    check(invoke(second, memory, b, process_service::processID) == GuestServiceAction::resume &&
          b.registers[0] == 1002, "second process PID differs");
    check(invoke(first, memory, a, process_service::open, 0xdeadbeef)
          == GuestServiceAction::memoryFault, "bad guest string pointer trapped");
    check(first.opens() == 1 && first.closes() == 1 && second.opens() == 1,
          "successful operation counters distinct");

    // Filesystem is read-only and neither environment has host filesystem APIs.
    auto traversal = memoryWith("/app/../secrets");
    check(invoke(first, traversal, a, process_service::open, 0x4000) == GuestServiceAction::resume &&
          a.registers[0] == process_service::failure, "guest traversal rejected by open");
    check(invoke(first, memory, a, process_service::open, 0x4000) == GuestServiceAction::resume &&
          a.registers[0] == 3, "closed descriptor number can be reused");
    for (unsigned i = 0; i < 15; ++i) {
        check(invoke(first, memory, a, process_service::open, 0x4000) == GuestServiceAction::resume &&
              a.registers[0] == i + 4, "allocate bounded guest descriptor");
    }
    check(first.openDescriptors() == 16, "maximum 16 descriptors per process");
    check(invoke(first, memory, a, process_service::open, 0x4000) == GuestServiceAction::resume &&
          a.registers[0] == process_service::failure, "descriptor limit enforced");
    check(second.openDescriptors() == 1, "descriptor exhaustion does not affect other process");

    auto diagnostic = runGuestProcessDiagnostic();
    check(bool(diagnostic), "two guest ELF binaries loaded");
    if (!diagnostic) return 1;
    check(diagnostic->completed, "process scheduler completed");
    check(diagnostic->results.size() == 2 && diagnostic->pids.size() == 2, "two simulated processes");
    check(diagnostic->pids[0] == 1001 && diagnostic->pids[1] == 1002, "correct simulated PIDs");
    check(diagnostic->results[0].stop == X64Stop::halted &&
          diagnostic->results[1].stop == X64Stop::halted, "both guest programs halted");
    check(diagnostic->results[0].rax() == 1001 && diagnostic->results[1].rax() == 1002,
          "guest processID service returns independent PID");
    check(diagnostic->results[0].stackRestored() && diagnostic->results[1].stackRestored(),
          "both independent guest stacks restored");
    check(diagnostic->opens == 2 && diagnostic->reads == 2 && diagnostic->closes == 2,
          "each process opened read and closed its own descriptor");
    check(diagnostic->writes == 2 && diagnostic->output == "HelloHello", "guest file bytes written from two buffers");
    check(diagnostic->yields == 2, "two explicit guest yields");
    check(diagnostic->dispatchOrder.size() >= 2 && diagnostic->dispatchOrder[0] == 1001 &&
          diagnostic->dispatchOrder[1] == 1002, "round robin starts with PID order");
    check(diagnostic->instructions > 20 && diagnostic->instructions < 100,
          "guest instruction count bounded");

    GuestProcessManager noGuest(vfs);
    check(!noGuest.run().completed, "no-process manager does not complete");
    check(!noGuest.addProcess(GuestMemory(), 0x1000), "unmapped guest entry refused");
    GuestProcessManager loopManager(vfs);
    check(loopManager.addProcess(singleInstruction({0xeb, 0xfe}), 0x1000),
          "looping process added");
    const auto limited = loopManager.run(1, 5);
    check(!limited.completed && limited.instructions == 5,
          "process scheduler enforces global instruction cap");
    check(!loopManager.run().completed, "already-run scheduler cannot restart");
    check(!loopManager.addProcess(singleInstruction({0xf4}), 0x1000),
          "already-run scheduler cannot add processes");
    GuestProcessManager cappedManager(vfs);
    check(cappedManager.addProcess(singleInstruction({0xf4}), 0x1000), "add PID 1001");
    check(cappedManager.addProcess(singleInstruction({0xf4}), 0x1000), "add PID 1002");
    check(cappedManager.addProcess(singleInstruction({0xf4}), 0x1000), "add PID 1003");
    check(cappedManager.addProcess(singleInstruction({0xf4}), 0x1000), "add PID 1004");
    check(!cappedManager.addProcess(singleInstruction({0xf4}), 0x1000), "process count max four");
    check(!cappedManager.run(0, 20).completed, "zero quantum rejected");
    check(cappedManager.run(1, 20).completed, "four processes complete");
    check(cappedManager.process(0) && cappedManager.process(0)->pid() == 1001,
          "process services accessible by index");
    check(cappedManager.process(4) == nullptr, "out-of-range process lookup safe");

    MisakiProcessReport bridge{};
    check(misaki_core_run_process_diagnostic(&bridge) == 0, "Swift C ABI process diagnostic success");
    check(bridge.abi_version == 1 && bridge.process_count == 2, "bridge process count");
    check(bridge.opens == 2 && bridge.reads == 2 && bridge.closes == 2, "bridge file operations");
    check(bridge.output_matches == 1 && bridge.output_bytes == 10, "bridge captured HelloHello");
    check(bridge.yields == 2 && bridge.stacks_restored == 1, "bridge yields and stacks");
    check(bridge.pid1_rax == 1001 && bridge.pid2_rax == 1002, "bridge guest process IDs");
    check(misaki_core_run_process_diagnostic(nullptr) == -1, "bridge requires result pointer");
    std::cout << "PASS: " << checks << " native guest-process assertions\n";
}
