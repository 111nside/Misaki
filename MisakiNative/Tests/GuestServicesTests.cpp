#include "GuestServices.hpp"
#include "GuestCPU.hpp"
#include "MisakiCoreBridge.h"

#include <cstdlib>
#include <iostream>
#include <limits>
#include <vector>

namespace {
int checks = 0;
void check(bool value, const char *message) {
    ++checks;
    if (!value) { std::cerr << "FAIL: " << message << '\n'; std::exit(EXIT_FAILURE); }
}
void put64(std::vector<std::uint8_t> &buf, std::uint64_t value) {
    for (int i = 0; i < 8; ++i) buf.push_back(static_cast<std::uint8_t>(value >> (i * 8)));
}
std::vector<std::uint8_t> movRAX(std::uint64_t value) {
    std::vector<std::uint8_t> program{0x48, 0xb8};
    put64(program, value);
    return program;
}
misaki::GuestMemory programMemory(const std::vector<std::uint8_t> &program,
                                   std::uint64_t address = 0x1000) {
    using namespace misaki;
    GuestMemory mem;
    check(mem.map(address, program.size(), permission::read | permission::write), "map test program");
    check(mem.writeBytes(address, program.data(), program.size()), "write test program");
    check(mem.protect(address, program.size(), permission::read | permission::execute), "protect test program");
    return mem;
}
}

int main() {
    using namespace misaki;
    const auto demo = runGuestServiceDiagnostic();
    check(bool(demo), "ELF guest scheduler fixture loaded");
    check(demo && demo->scheduler.completed, "guest schedule complete");
    check(demo && demo->scheduler.threadResults.size() == 2, "two isolated guest contexts");
    check(demo && demo->scheduler.instructions == 30, "30 guest instructions executed");
    check(demo && demo->scheduler.yieldEvents == 2, "two cooperative yields");
    check(demo && demo->calls == 8, "eight guest service calls");
    check(demo && demo->writes == 2, "two guest writes");
    check(demo && demo->output == "OKOK", "guest output captured in service environment");
    check(demo && demo->scheduler.threadResults[0].rax() == 4097, "first guest received TID 1");
    check(demo && demo->scheduler.threadResults[1].rax() == 4098, "second guest received TID 2");
    check(demo && demo->scheduler.threadResults[0].stop == X64Stop::halted &&
              demo->scheduler.threadResults[1].stop == X64Stop::halted, "both guest threads halted");
    check(demo && demo->scheduler.threadResults[0].stackRestored() &&
              demo->scheduler.threadResults[1].stackRestored(), "two guest stacks restored");
    check(demo && demo->scheduler.dispatchOrder.size() > 2 &&
              demo->scheduler.dispatchOrder[0] == 1 &&
              demo->scheduler.dispatchOrder[1] == 2, "round-robin dispatch order");

    // Unsupported service must stop with an explicit error.
    auto invalid = movRAX(0xdead);
    invalid.insert(invalid.end(), {0x0f, 0x05, 0xf4});
    GuestServiceEnvironment services;
    PortableX64Backend unknown(programMemory(invalid), 0x1000, &services);
    check(unknown.run(10).stop == X64Stop::unsupportedSyscall, "unknown guest service rejected");
    check(services.output().empty(), "rejected service cannot write output");

    // No dispatcher must not accidentally use a real host syscall.
    PortableX64Backend noServices(programMemory(invalid), 0x1000);
    check(noServices.run(10).stop == X64Stop::unsupportedSyscall, "syscall without dispatcher rejected");

    // A bad guest pointer cannot append partially to host output.
    auto writeBad = movRAX(guest_service::write);
    writeBad.insert(writeBad.end(), {0x0f, 0x05, 0xf4});
    auto badMemory = programMemory(writeBad);
    X64State fake;
    fake.registers[0] = guest_service::write;
    fake.registers[7] = 0x8000;
    fake.registers[6] = 3;
    check(services.dispatch(badMemory, fake, 1) == GuestServiceAction::memoryFault,
          "unmapped guest write rejected");
    check(services.output().empty(), "unmapped write atomic");
    fake.registers[7] = std::numeric_limits<std::uint64_t>::max();
    fake.registers[6] = 2;
    check(services.dispatch(badMemory, fake, 1) == GuestServiceAction::memoryFault,
          "overflowing guest pointer rejected");
    fake.registers[7] = 0x1000;
    fake.registers[6] = 4097;
    check(services.dispatch(badMemory, fake, 1) == GuestServiceAction::memoryFault,
          "oversized guest write rejected");

    // Exit service terminates guest context and preserves status by thread ID.
    auto exitProgram = movRAX(guest_service::exit);
    exitProgram.insert(exitProgram.end(), {0x0f, 0x05, 0xf4});
    GuestServiceEnvironment exiting;
    PortableX64Backend thread(programMemory(exitProgram), 0x1000, &exiting, 7);
    const auto exitResult = thread.run(10);
    check(exitResult.stop == X64Stop::exited, "guest exit service terminates program");
    check(exitResult.state.instructions == 2, "exit syscall is included in count");
    check(exiting.exitCodes().count(7) == 1, "guest exit attributed to thread");
    check(thread.run(10).stop == X64Stop::exited, "terminated guest cannot resume");

    // Instruction cap prevents a looping guest from monopolizing the scheduler.
    GuestRoundRobinScheduler capped;
    check(capped.addThread(programMemory({0xeb, 0xfe}), 0x1000), "register looping guest");
    const auto limit = capped.run(1, 5);
    check(!limit.completed && limit.instructions == 5, "bounded cooperative loop");
    check(!capped.run().completed, "scheduler cannot be reused after completion attempt");
    check(!capped.addThread(programMemory({0xf4}), 0x1000), "cannot mutate running scheduler");
    GuestRoundRobinScheduler invalidConfiguration;
    check(!invalidConfiguration.addThread(GuestMemory(), 0x1000), "reject unmapped entrypoint");
    check(!invalidConfiguration.run().completed, "empty scheduler cannot complete");

    MisakiServiceThreadReport bridge{};
    check(misaki_core_run_services_diagnostic(&bridge) == 0, "C bridge service diagnostics");
    check(bridge.abi_version == 1 && bridge.thread_count == 2, "C bridge threads");
    check(bridge.instructions == 30 && bridge.yields == 2, "C bridge instruction/yield counts");
    check(bridge.service_calls == 8 && bridge.write_calls == 2, "C bridge syscall counts");
    check(bridge.output_bytes == 4 && bridge.output_matches == 1, "C bridge output check");
    check(bridge.threads_halted == 1 && bridge.stacks_restored == 1, "C bridge halt/stack checks");
    check(bridge.thread1_rax == 4097 && bridge.thread2_rax == 4098, "C bridge per-thread results");
    check(misaki_core_run_services_diagnostic(nullptr) == -1, "C bridge validates output pointer");

    std::cout << "PASS: " << checks << " native guest-service assertions\n";
}
