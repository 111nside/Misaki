#include "../Include/GuestSCE52.hpp"
#include "../Include/GuestSCE51.hpp"
#include <algorithm>
#include <cctype>
namespace misaki { namespace {
std::uint64_t readLE(const std::uint8_t *b, std::size_t offset, unsigned size) {
    std::uint64_t value=0;
    for(unsigned i=0;i<size;++i) value |= std::uint64_t(b[offset+i]) << (8*i);
    return value;
}
bool safeName(const std::string &s, std::size_t max) {
    if(s.empty() || s.size()>max) return false;
    return std::all_of(s.begin(),s.end(),[](char c){
        const auto u=static_cast<unsigned char>(c);
        return std::isalnum(u) || c=='_' || c=='.' || c=='-';
    });
}
} // namespace
std::optional<SCE52Symbol> parseSCE52Symbol(const std::string &s) {
    if (s.size()>150) return std::nullopt;
    const auto a=s.find('#');
    if(a!=11) return std::nullopt;
    const auto b=s.find('#',a+1);
    if(b==std::string::npos || s.find('#',b+1)!=std::string::npos)
        return std::nullopt;
    SCE52Symbol out{s.substr(0,a),s.substr(a+1,b-a-1),s.substr(b+1)};
    if (!safeName(out.library,64) || !safeName(out.module,64)) return std::nullopt;
    for(char c:out.encodedNid) {
        const auto u=static_cast<unsigned char>(c);
        if (!std::isalnum(u) && c!='+' && c!='-') return std::nullopt;
    }
    return out;
}
std::optional<SCE52Tags> inspectSCE52Dynamic(const std::uint8_t *b,
                                             std::size_t size,std::string *error) {
    if(error) error->clear();
    const auto fail=[&](const char *msg)->std::optional<SCE52Tags>{
        if(error)*error=msg;return std::nullopt;
    };
    auto parsed=inspectSCE51(b,size,error);
    if(!parsed || parsed->container!=PS4ContainerKind::sceELF)
        return fail("SCE dynamic metadata requires unwrapped ELF");
    auto it=std::find_if(parsed->segments.begin(),parsed->segments.end(),
                        [](const SCE51Segment &s){return s.type==2;});
    if(it==parsed->segments.end() || it->fileBytes==0 || it->fileBytes>4096 ||
       it->fileBytes%16 || it->offset>size || it->fileBytes>size-it->offset)
        return fail("Invalid PT_DYNAMIC bounds");
    SCE52Tags tags;
    bool seenModuleInfo=false;
    for(std::uint64_t i=0;i<it->fileBytes;i+=16) {
        const auto at=static_cast<std::size_t>(it->offset+i);
        const auto tag=readLE(b,at,8), value=readLE(b,at+8,8);
        if(tag==0){tags.ended=true;break;}
        ++tags.entries;
        if(tag>=0x61000000u && tag<=0x6100004fu) {
            ++tags.sceTags;
            if(tag==0x6100000du) { // DT_SCE_MODULE_INFO
                if(seenModuleInfo)return fail("Duplicate SCE module-info tag");
                seenModuleInfo=true;tags.moduleInfo=value;
            } else if(tag==0x6100000fu) { // DT_SCE_NEEDED_MODULE
                if(!tags.neededModules) tags.firstNeeded=value;
                if(++tags.neededModules>64) return fail("Too many needed SCE modules");
            } else if(tag==0x61000015u) ++tags.importLibraries;
        }
        if(tags.entries>256) return fail("Too many ELF dynamic entries");
    }
    if(!tags.ended) return fail("Unterminated dynamic table");
    return tags;
}
std::optional<SCE52Resolution> resolveSCE52(
    const std::vector<PS4ImportKey>&keys,const PS4ImportCatalog &catalog) {
    if(keys.empty() || keys.size()>64) return std::nullopt;
    std::vector<std::string> seen;
    SCE52Resolution result;
    for(const auto &key:keys) {
        if(!key.major || key.nid.size()!=11 || !safeName(key.library,64) ||
           !safeName(key.module,64) || !parseSCE52Symbol(
               key.nid+"#"+key.library+"#"+key.module)) return std::nullopt;
        const auto identity=key.nid+"#"+key.library+"#"+key.module;
        if(std::find(seen.begin(),seen.end(),identity)!=seen.end())return std::nullopt;
        seen.push_back(identity);
        const auto address=catalog.resolve(key);
        if(!address)return std::nullopt;
        if(!result.matched)result.firstAddress=*address;
        ++result.matched;
    }
    return result;
}
} // namespace misaki
