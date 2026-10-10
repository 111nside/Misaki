#include "../Include/MisakiGPUBridge.h"
#include "../Include/GuestGPU.hpp"

extern "C" int32_t misaki_gpu_demo_frame(MisakiGPUCommand *output, size_t capacity,
                                            MisakiGPUFrameInfo *info) {
    if (!info || !output) return -1;
    *info = MisakiGPUFrameInfo{};
    try {
        const auto frame = misaki::makeDemoGPUFrame();
        const auto check = misaki::validateGPUFrame(frame);
        if (!check || *check != frame.checksum) return -2;
        if (capacity < frame.commands.size()) return -3;
        std::uint32_t rectangles = 0;
        for (std::size_t i = 0; i < frame.commands.size(); ++i) {
            const auto &c = frame.commands[i];
            output[i] = {static_cast<uint32_t>(c.opcode), c.x, c.y,
                         c.width, c.height, c.rgba};
            if (c.opcode == misaki::GPUOpcode::rectangle) ++rectangles;
        }
        info->abi_version = 1;
        info->width = frame.width;
        info->height = frame.height;
        info->command_count = static_cast<uint32_t>(frame.commands.size());
        info->rectangle_count = rectangles;
        info->clear_count = 1;
        info->present_count = 1;
        info->checksum = frame.checksum;
        return 0;
    } catch (...) {
        return -2;
    }
}
