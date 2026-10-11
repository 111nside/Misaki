#include "../Include/GuestServices57.hpp"
#include "../Include/GuestSCE52.hpp"
#include <vector>
namespace misaki {
bool ServiceRegistry57::add(ServiceBinding57 item){
    const auto &k=item.key;
    if(!k.major || k.nid.size()!=11 || !parseSCE52Symbol(k.nid+"#"+k.library+"#"+k.module))return false;
    return entries_.emplace(std::move(item.key),item.action).second;
}
std::optional<PrivateService57> ServiceRegistry57::resolve(const PS4ImportKey &key)const{
    const auto it=entries_.find(key);
    if(it==entries_.end())return std::nullopt;
    return it->second;
}
std::optional<Service57Result> runService57Diagnostic(){
    ServiceRegistry57 registry;
    auto a=makePS4SyntheticKey(0),b=makePS4SyntheticKey(1);
    if(!registry.add({a,PrivateService57::pid})||
       !registry.add({b,PrivateService57::write})||registry.size()!=2||
       registry.resolve(a)!=PrivateService57::pid||
       registry.resolve(b)!=PrivateService57::write)return std::nullopt;
    Service57Result r;r.resolved=2;
    GuestMemory memory;
    const std::uint8_t hello[]={'H','e','l','l','o'};
    if(!memory.map(0x310000,sizeof(hello),permission::read|permission::write)||
       !memory.writeBytes(0x310000,hello,sizeof(hello))||
       !memory.protect(0x310000,sizeof(hello),permission::read))return std::nullopt;
    SCEKernelShim service(1234,1000);
    X64State state{};
    auto call=[&](const PS4ImportKey &key,std::uint32_t thread)->GuestServiceAction {
        const auto action=registry.resolve(key);
        if(!action)return GuestServiceAction::unsupported;
        state.registers[0]=*action==PrivateService57::pid?sce_shim::pid:
            *action==PrivateService57::write?sce_shim::write:
            *action==PrivateService57::monotonicClock?sce_shim::clock:sce_shim::exit;
        return service.dispatch(memory,state,thread);
    };
    if(call(a,1234)!=GuestServiceAction::resume || state.registers[0]!=1234)return std::nullopt;
    state.registers[7]=0x310000;state.registers[6]=sizeof(hello);
    if(call(b,1234)!=GuestServiceAction::resume ||state.registers[0]!=5 ||service.output().size()!=5)return std::nullopt;
    // A foreign process must never receive this process's service privilege.
    if(call(a,1235)!=GuestServiceAction::resume ||state.registers[0]!=sce_shim::failure)return std::nullopt;
    r.calls=service.calls();r.failures=service.failures();
    r.outputBytes=static_cast<std::uint32_t>(service.output().size());
    r.processIsolated=true;
    return r;
}
} // namespace misaki
