#include "../Include/GuestSCEKernel.hpp"
#include <limits>
namespace misaki {
SCEKernelShim::SCEKernelShim(std::uint32_t pid, std::uint64_t ticks)
    : pid_(pid), nextTick_(ticks) {}
GuestServiceAction SCEKernelShim::dispatch(GuestMemory &memory, X64State &state,
                                           std::uint32_t threadID) {
    ++calls_;
    const auto op=state.registers[0];
    const auto invalid=[&]() {
        ++failures_;
        state.registers[0]=sce_shim::failure;
        return GuestServiceAction::resume;
    };
    if (exited_ || threadID != pid_) return invalid();
    switch (op) {
    case sce_shim::pid:
        lastPID_=pid_;
        state.registers[0]=pid_;
        return GuestServiceAction::resume;
    case sce_shim::clock:
        if (nextTick_ == UINT64_MAX) return invalid();
        lastTicks_=nextTick_++;
        state.registers[0]=lastTicks_;
        return GuestServiceAction::resume;
    case sce_shim::write: {
        const auto address=state.registers[7], length=state.registers[6];
        if (length==0 || length>256 || address>UINT64_MAX-length ||
            output_.size()>1024-length) return invalid();
        std::vector<std::uint8_t> bytes;
        bytes.reserve(static_cast<std::size_t>(length));
        for (std::uint64_t i=0; i<length; ++i) {
            const auto v=memory.read8(address+i);
            if (!v) return invalid();
            bytes.push_back(*v);
        }
        output_.insert(output_.end(),bytes.begin(),bytes.end());
        state.registers[0]=length;
        return GuestServiceAction::resume;
    }
    case sce_shim::exit:
        exitCode_=state.registers[7];
        if (exitCode_>255) return invalid();
        exited_=true;
        state.registers[0]=exitCode_;
        return GuestServiceAction::exit;
    default: return invalid();
    }
}
namespace {
void mov(std::vector<std::uint8_t> &v, std::uint8_t reg, std::uint64_t n) {
    v.push_back(0x48); v.push_back(std::uint8_t(0xb8+reg));
    for (unsigned i=0;i<8;++i) v.push_back(std::uint8_t(n>>(8*i)));
}
void syscall(std::vector<std::uint8_t> &v) { v.push_back(0x0f);v.push_back(0x05); }
}
std::vector<std::uint8_t> makeSCEKernelGuestFixture() {
    std::vector<std::uint8_t> v;
    mov(v,0,sce_shim::pid); syscall(v);
    mov(v,0,sce_shim::clock); syscall(v);
    mov(v,7,0x300000); mov(v,6,5); mov(v,0,sce_shim::write); syscall(v);
    mov(v,7,0); mov(v,0,sce_shim::exit); syscall(v);
    return v;
}
} // namespace misaki
