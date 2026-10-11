#include "../Include/GuestPersistent59.hpp"
#include <set>
namespace misaki {
namespace {
constexpr std::uint64_t pidCall=0x9951,yieldCall=0x9952,clockCall=0x9953,exitCall=0x9954;
struct Service final:IGuestServiceDispatcher{
    std::uint32_t pid=0,calls=0,yields=0;
    bool exited=false;
    explicit Service(std::uint32_t p):pid(p){}
    GuestServiceAction dispatch(GuestMemory&,X64State &s,std::uint32_t thread)override{
        ++calls;
        if(thread!=pid || exited)return GuestServiceAction::unsupported;
        const auto id=s.registers[0];
        if(id==pidCall){s.registers[0]=pid;return GuestServiceAction::resume;}
        if(id==yieldCall){s.registers[0]=0;++yields;return GuestServiceAction::yield;}
        if(id==clockCall){s.registers[0]=10000+calls;return GuestServiceAction::resume;}
        if(id==exitCall){exited=true;s.registers[0]=0;return GuestServiceAction::exit;}
        return GuestServiceAction::unsupported;
    }
};
void mov(std::vector<std::uint8_t> &b,std::uint64_t value){b.push_back(0x48);b.push_back(0xb8);for(unsigned i=0;i<8;++i)b.push_back(static_cast<std::uint8_t>(value>>(8*i)));}
}
struct PersistentGuest59::Slot{
    Service service;
    std::unique_ptr<PortableX64Backend> cpu;
    std::uint32_t lastInstructions=0;
    bool done=false;
    explicit Slot(std::uint32_t pid):service(pid){}
};
PersistentGuest59::PersistentGuest59()=default;
PersistentGuest59::~PersistentGuest59()=default;
std::vector<std::uint8_t> makePersistent59Program(){
    std::vector<std::uint8_t>b;
    for(auto id:{pidCall,yieldCall,clockCall,exitCall}){
        mov(b,id);b.push_back(0x0f);b.push_back(0x05);
    }
    return b;
}
bool PersistentGuest59::add(std::uint32_t pid,const std::vector<std::uint8_t>&code){
    if(bad_||pid==0||slots_.size()>=4||code.empty()||code.size()>512)return false;
    for(const auto &s:slots_)if(s->service.pid==pid)return false;
    GuestMemory memory;
    const auto address=0x10000ull+std::uint64_t(slots_.size())*0x10000;
    if(!memory.map(address,code.size(),permission::read|permission::write)||
       !memory.writeBytes(address,code.data(),code.size())||
       !memory.protect(address,code.size(),permission::read|permission::execute))return false;
    auto slot=std::make_unique<Slot>(pid);
    slot->cpu=std::make_unique<PortableX64Backend>(std::move(memory),address,&slot->service,pid);
    slots_.push_back(std::move(slot));
    return true;
}
bool PersistentGuest59::step(std::uint32_t quantum){
    if(bad_||slots_.empty()||quantum==0||quantum>32)return false;
    if(allExited())return true;
    for(auto &slot:slots_){
        if(slot->done)continue;
        const auto result=slot->cpu->run(quantum);
        if(result.state.instructions<slot->lastInstructions){bad_=true;return false;}
        const auto delta=result.state.instructions-slot->lastInstructions;
        if(instructions_>100000-delta){bad_=true;return false;}
        instructions_+=delta;slot->lastInstructions=result.state.instructions;
        ++switches_;
        if(result.stop==X64Stop::exited){slot->done=true;}
        else if(result.stop!=X64Stop::stepLimit && result.stop!=X64Stop::yielded){bad_=true;return false;}
    }
    return true;
}
bool PersistentGuest59::allExited()const{
    if(slots_.empty())return false;
    for(const auto &s:slots_)if(!s->done)return false;
    return true;
}
std::uint32_t PersistentGuest59::processes()const{return static_cast<std::uint32_t>(slots_.size());}
std::uint32_t PersistentGuest59::yields()const{std::uint32_t n=0;for(const auto&s:slots_)n+=s->service.yields;return n;}
std::uint32_t PersistentGuest59::serviceCalls()const{std::uint32_t n=0;for(const auto&s:slots_)n+=s->service.calls;return n;}
std::uint32_t PersistentGuest59::exited()const{std::uint32_t n=0;for(const auto&s:slots_)n+=s->done?1u:0u;return n;}
std::optional<Persistent59Report> runPersistent59Diagnostic(){
    PersistentGuest59 runtime;
    const auto code=makePersistent59Program();
    if(!runtime.add(3001,code)||!runtime.add(3002,code))return std::nullopt;
    Persistent59Report report;
    for(std::uint32_t i=0;i<24&&!runtime.allExited();++i){
        if(!runtime.step(2))return std::nullopt;
        ++report.ticks;
    }
    if(!runtime.allExited())return std::nullopt;
    report.processes=runtime.processes();report.instructions=runtime.instructions();
    report.contextSwitches=runtime.contextSwitches();
    report.yields=runtime.yields();report.calls=runtime.serviceCalls();report.exited=runtime.exited();
    if(report.processes!=2||report.yields!=2||report.calls!=8||report.exited!=2||report.instructions!=16)
        return std::nullopt;
    return report;
}
} // namespace misaki
