#include "../Include/GuestSCE47.hpp"
#include <functional>
#include <map>
#include <set>
#include <utility>
namespace misaki {
std::optional<SCE47Result> bindSCE47Graph(const std::vector<SCE47Module>&modules,
     const PS4ImportCatalog &catalog,GuestMemory &memory,std::string *error) {
    if(error)error->clear();
    const auto reject=[&](const char*reason)->std::optional<SCE47Result>{
        if(error)*error=reason;return std::nullopt;};
    if(modules.empty()||modules.size()>16)return reject("Module dependency count outside budget");
    std::map<std::string,std::size_t> names;
    for(std::size_t i=0;i<modules.size();++i) {
        const auto &m=modules[i];
        if(m.name.size()<3 || m.name.size()>64 || !m.major ||
           m.dependencies.size()>12 || m.slots.size()>16 ||
           (m.slots.empty() && m.regionBytes!=0) ||
           (!m.slots.empty() && (m.regionBytes<8 || m.regionBytes>4096 ||
             m.region>UINT64_MAX-m.regionBytes)) ||
           !names.emplace(m.name,i).second)
            return reject("Malformed or duplicate synthetic SCE module");
    }
    // Strict DFS detects cycles and missing modules before touching GuestMemory.
    std::vector<std::uint8_t> state(modules.size(),0);
    std::vector<std::size_t> loadOrder;
    std::function<bool(std::size_t)> visit=[&](std::size_t i)->bool{
        if(state[i]==2)return true;
        if(state[i]==1)return false;
        state[i]=1;
        std::set<std::string> deps;
        for(const auto &d:modules[i].dependencies){
            if(!deps.insert(d).second)return false;
            auto it=names.find(d);
            if(it==names.end() || !visit(it->second))return false;
        }
        state[i]=2;loadOrder.push_back(i);return true;
    };
    for(std::size_t i=0;i<modules.size();++i)if(!visit(i))
        return reject("Missing, duplicated or circular SCE module dependency");
    GuestMemory scratch=memory;
    SCE47Result result;
    for(auto i:loadOrder){
        const auto &m=modules[i];
        if(!m.slots.empty()) {
            // Version checks are exact. No best-match fallback or host symbol lookup.
            for(const auto &s:m.slots){
                if(!s.symbol.major || s.symbol.major!=m.major ||
                    s.symbol.minor!=m.minor)return reject("SCE NID version mismatch");
            }
            const auto bound=bindSCEImportSlots(catalog,m.slots,m.region,m.regionBytes,scratch,error);
            if(!bound)return std::nullopt;
            result.imports+=bound->bindings;
        }
        result.loadOrder.push_back(m.name);
    }
    result.modules=static_cast<std::uint32_t>(modules.size());
    result.sealed=true;
    memory=std::move(scratch);
    return result;
}
std::vector<SCE47Module> makeSCE47Modules(){
    SCE47Module core;core.name="libSceDemoCore";core.major=1;core.minor=0;
    core.region=0x150000;core.regionBytes=0x20;
    core.slots={{makePS4SyntheticKey(0),0x150000}};
    SCE47Module gfx;gfx.name="libSceDemoGraphics";gfx.major=1;gfx.minor=0;
    gfx.dependencies={core.name};gfx.region=0x151000;gfx.regionBytes=0x20;
    gfx.slots={{makePS4SyntheticKey(1),0x151000}};
    return {gfx,core}; // deliberately unordered; DFS resolves dependency first
}
}
