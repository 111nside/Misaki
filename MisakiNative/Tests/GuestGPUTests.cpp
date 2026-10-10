#include "GuestGPU.hpp"
#include "MisakiGPUBridge.h"

#include <cstdlib>
#include <iostream>
#include <limits>
#include <vector>

namespace {
int checks = 0;
void check(bool ok, const char *message) {
    ++checks;
    if (!ok) {
        std::cerr << "FAIL: " << message << '\n';
        std::exit(EXIT_FAILURE);
    }
}
}
int main() {
    using namespace misaki;
    auto demo = makeDemoGPUFrame();
    check(demo.width == 320 && demo.height == 180, "scene dimensions");
    check(demo.commands.size() == 7, "clear + five rectangles + present");
    check(demo.commands.front().opcode == GPUOpcode::clear, "clear first");
    check(demo.commands.back().opcode == GPUOpcode::present, "present last");
    check(demo.checksum != 0, "checksum present");
    check(validateGPUFrame(demo) == demo.checksum, "checksum validates");
    check(validateGPUFrame(makeDemoGPUFrame()) == demo.checksum, "deterministic output");

    GuestGPUBuilder b(40, 20);
    check(!b.rectangle(0, 0, 10, 10, 0xFFFFFFFF), "clear required");
    check(!b.present(), "present requires clear");
    check(!b.clear(0xFFFFFF00), "reject nonopaque clear");
    check(b.clear(0x000000FF), "accept opaque clear");
    check(!b.clear(0x000000FF), "single clear");
    check(!b.rectangle(0, 0, 0, 10, 0xFFFFFFFF), "reject zero width");
    check(!b.rectangle(40, 0, 1, 1, 0xFFFFFFFF), "reject starting outside");
    check(!b.rectangle(0, 20, 1, 1, 0xFFFFFFFF), "reject y outside");
    check(!b.rectangle(35, 5, 6, 3, 0xFFFFFFFF), "reject width beyond bounds");
    check(!b.rectangle(0, 15, 1, 6, 0xFFFFFFFF), "reject height beyond bounds");
    check(!b.rectangle(std::numeric_limits<uint32_t>::max(), 0, 3, 3, 0xFFFFFFFF), "overflow-safe x");
    check(!b.rectangle(1, 1, 2, 2, 0x22334455), "opaque-only rectangles");
    check(!b.finish(), "cannot finish before present");
    check(b.rectangle(0, 0, 40, 20, 0x123456FF), "fullscreen rect");
    check(b.present(), "present after commands");
    check(!b.present(), "reject repeat present");
    check(!b.rectangle(1, 1, 1, 1, 0xFFFFFFFF), "reject post-present drawing");
    check(bool(b.finish()), "finalized frame valid");

    GuestGPUBuilder bad(0, 20);
    check(!bad.clear(0xFFFFFFFF), "reject zero canvas");
    GuestGPUBuilder huge(4096, 4096);
    check(!huge.clear(0xFFFFFFFF), "cap canvas");
    GuestGPUBuilder full(10, 10);
    check(full.clear(0xFFFFFFFF), "start capacity test");
    for (int i = 0; i < 62; ++i)
        check(full.rectangle(0, 0, 1, 1, 0xFFFFFFFF), "within command cap");
    check(!full.rectangle(0, 0, 1, 1, 0xFFFFFFFF), "reject 64th draw");
    check(full.present(), "exact maximum commands");
    check(full.finish()->commands.size() == 64, "command cap includes present");

    auto corrupt = demo;
    corrupt.commands[0].opcode = GPUOpcode::rectangle;
    check(!validateGPUFrame(corrupt), "invalid opcode ordering");
    corrupt = demo;
    corrupt.commands.back().rgba = 1;
    check(!validateGPUFrame(corrupt), "reject present payload");
    corrupt = demo;
    corrupt.commands[2].width = std::numeric_limits<uint32_t>::max();
    check(!validateGPUFrame(corrupt), "reject invalid rect on untrusted frame");
    corrupt = demo;
    corrupt.commands.insert(corrupt.commands.end() - 1, corrupt.commands[0]);
    check(!validateGPUFrame(corrupt), "reject second clear");
    corrupt = demo;
    corrupt.commands.push_back({GPUOpcode::present, 0,0,0,0,0});
    check(!validateGPUFrame(corrupt), "reject duplicate present");

    MisakiGPUFrameInfo info{};
    MisakiGPUCommand commands[16]{};
    check(misaki_gpu_demo_frame(commands, 16, &info) == 0, "C bridge returns frame");
    check(info.abi_version == 1, "ABI v1");
    check(info.width == 320 && info.height == 180, "C sizes");
    check(info.command_count == 7, "C count");
    check(info.rectangle_count == 5 && info.clear_count == 1 && info.present_count == 1,
          "C counts");
    check(info.checksum == demo.checksum, "C/native checksum agree");
    check(commands[0].kind == 1 && commands[6].kind == 3, "C first and last opcodes");
    check(commands[1].x == 24 && commands[1].width == 272, "C rectangle payload");
    commands[0].kind = 99;
    check(misaki_gpu_demo_frame(commands, 1, &info) == -3, "small buffer rejected");
    check(commands[0].kind == 99, "rejection does not partially overwrite");
    check(info.command_count == 0, "error clears report");
    check(misaki_gpu_demo_frame(nullptr, 16, &info) == -1, "null commands rejected");
    check(misaki_gpu_demo_frame(commands, 16, nullptr) == -1, "null info rejected");
    std::cout << "PASS: " << checks << " native GPU assertions\n";
}
