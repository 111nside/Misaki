#include "GuestIntegratedRuntime.hpp"
#include <cstdlib>
#include <iostream>
#include <vector>
using namespace misaki;
static unsigned checks = 0;
static void check(bool ok, const char *why) { ++checks; if (!ok) { std::cerr << "FAIL: " << why << '\n'; std::exit(1); } }
static GuestMemory makeProgram(unsigned tag) {
    GuestMemory memory;
    const auto bytes = makeRuntime31Program(tag);
    check(memory.map(0x2000, bytes.size(), permission::read | permission::write), "map code");
    check(memory.writeBytes(0x2000, bytes.data(), bytes.size()), "write code");
    check(memory.protect(0x2000, bytes.size(), permission::read | permission::execute), "protect code");
    return memory;
}
int main() {
    IntegratedGuestRuntime engine;
    check(!engine.addFile("../host", {'X'}), "reject noncanonical path");
    check(engine.addFile("/virtual/data", {'A','B'}), "fixture filesystem");
    check(engine.addProcess(makeProgram(71), 0x2000), "add isolated process one");
    check(engine.addProcess(makeProgram(72), 0x2000), "add isolated process two");
    check(!engine.addFile("/late", {'X'}), "prevent late mutations");
    check(engine.processCount() == 2, "two independent processes");
    check(engine.services(0)->pid() == 1001 && engine.services(1)->pid() == 1002,
          "stable per-process PIDs");
    check(engine.services(2) == nullptr, "out of range service lookup");
    const auto result = engine.run(1, 200);
    check(result.completed && result.processes.size() == 2, "interleaved scheduler completes");
    check(result.contextSwitches >= 8 && result.instructions > 4, "cooperative context switches");
    for (unsigned i = 0; i < 2; ++i) {
        const auto &p = result.processes[i];
        check(p.pid == 1001 + i && p.execution.rax() == p.pid, "process-specific PID syscall");
        check(p.execution.stop == X64Stop::halted && p.execution.stackRestored(), "guest exit and stack restored");
        check(p.execution.state.registers[3] == 71 + i, "private registers preserved");
    }
    check(!engine.run().completed, "cannot execute already-finished runtime twice");
    check(!engine.addProcess(makeProgram(5), 0x2000), "cannot add process after start");
    IntegratedGuestRuntime budget;
    check(budget.addProcess(makeProgram(1), 0x2000), "budget runtime loads");
    check(!budget.run(1, 1).completed, "bounded instruction limit");
    IntegratedGuestRuntime invalid;
    check(!invalid.addProcess(GuestMemory{}, 0x1000), "unmapped entry rejected");
    check(!invalid.run().completed, "empty run rejected");
    std::cout << "PASS: " << checks << " integrated runtime 31 assertions\n";
}
