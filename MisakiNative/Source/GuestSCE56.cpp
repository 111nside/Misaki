#include "../Include/GuestSCE56.hpp"
#include "../Include/GuestSCE46.hpp"
#include <algorithm>
#include <set>
#include <limits>

namespace misaki { namespace {
std::uint64_t rd(const std::uint8_t *p,std::size_t at,unsigned n){std::uint64_t v=0;for(unsigned j=0;j<n;++j)v|=std::uint64_t(p[at+j])<<(8*j);return v;}
void wr(std::vector<std::uint8_t>&p,std::size_t at,std::uint64_t v,unsigned n){for(unsigned j=0;j<n;++j)p[at+j]=static_cast<std::uint8_t>(v>>(8*j));}
bool range(std::uint64_t off,std::uint64_t count,std::size_t len){return off<=len && count<=len-off;}
bool safeName(const std::string &s){
    if(s.empty()||s.size()>95)return false;
    for(unsigned char c:s)if(!((c>='a'&&c<='z')||(c>='A'&&c<='Z')||(c>='0'&&c<='9')||c=='_'||c=='.'||c=='-'||c=='+'))return false;
    return true;
}
} // namespace
std::optional<SCE56Dynamic> inspectSCE56Dynamic(const std::uint8_t *b,
                                                 std::size_t len,std::string *err){
    if(err)err->clear();
    const auto fail=[&](const char *reason)->std::optional<SCE56Dynamic>{if(err)*err=reason;return std::nullopt;};
    if(!b||len<64||len>8*1024*1024)return fail("Invalid SCE metadata bytes or size");
    const auto elf=inspectSCE51(b,len,err);
    if(!elf||elf->container!=PS4ContainerKind::sceELF)return fail("Dynamic inspection requires an unwrapped SCE ELF");
    const auto it=std::find_if(elf->segments.begin(),elf->segments.end(),[](const SCE51Segment &s){return s.type==2;});
    if(it==elf->segments.end()||it->fileBytes<16||it->fileBytes>4096||it->fileBytes%16||
       !range(it->offset,it->fileBytes,len))return fail("Missing or unsupported PT_DYNAMIC");
    struct Segment {std::uint64_t file,va,n;};
    std::vector<Segment> segments;
    for(const auto &s:elf->segments)if(s.type==1 && s.fileBytes){
        if(!range(s.offset,s.fileBytes,len))return fail("File-backed PT_LOAD outside file");
        segments.push_back({s.offset,s.virtualAddress,s.fileBytes});
    }
    auto locate=[&](std::uint64_t va,std::uint64_t n)->std::optional<std::uint64_t>{
        for(const auto &s:segments)if(va>=s.va && n<=s.n && va-s.va<=s.n-n)
            return s.file+(va-s.va);
        return std::nullopt;
    };
    SCE56Dynamic report;report.loadSegments=elf->loadSegments;
    std::uint64_t strVA=0,strLen=0,relaVA=0,relaLen=0,relaEnt=0;
    std::vector<std::uint64_t> offsets;
    std::set<std::uint64_t> seenSingleton;
    bool terminated=false;
    for(std::uint64_t i=0;i<it->fileBytes;i+=16){
        const auto at=static_cast<std::size_t>(it->offset+i);
        const auto tag=rd(b,at,8),value=rd(b,at+8,8);
        if(tag==0){terminated=true;break;}
        if(++report.entries>256)return fail("Dynamic entry budget exceeded");
        if(tag==1){if(offsets.size()>=32)return fail("DT_NEEDED budget exceeded");offsets.push_back(value);}
        else if(tag==5||tag==10||tag==7||tag==8||tag==9){
            if(!seenSingleton.insert(tag).second)return fail("Duplicate dynamic metadata tag");
            if(tag==5) strVA=value;
            if(tag==10) strLen=value;
            if(tag==7) relaVA=value;
            if(tag==8) relaLen=value;
            if(tag==9) relaEnt=value;
        }else if(tag==0x6100000f){if(report.sceNeededIdentifiers.size()>=32)return fail("Too many SCE dependency identifiers");report.sceNeededIdentifiers.push_back(value);}
        else if(tag>=0x61000000 && tag<=0x6100004f){ /* known namespace, unknown tag retained as metadata only */ }
        else if(tag==2||tag==3||tag==4||tag==6||tag==11||tag==12||tag==13||tag==14||tag==15||tag==16||tag==17||tag==18||tag==19||tag==20||tag==21||tag==22||tag==23||tag==24||tag==25||tag==26||tag==27||tag==28||tag==29||tag==30||tag==0x6ffffffb){
            report.hasUnsupportedRelocations=true; // conservative marker: semantics not implemented
        }
        else report.hasUnsupportedRelocations=true;
    }
    if(!terminated)return fail("Unterminated PT_DYNAMIC table");
    if(strVA||strLen||!offsets.empty()){
        if(!strVA||!strLen||strLen>4096)return fail("Missing or oversized dynamic string table");
        const auto file=locate(strVA,strLen);
        if(!file||!range(*file,strLen,len))return fail("Dynamic string table outside PT_LOAD");
        report.hasStringTable=true;
        std::set<std::string> unique;
        for(auto nameOff:offsets){
            if(nameOff>=strLen)return fail("DT_NEEDED string offset out of range");
            const auto first=static_cast<std::size_t>(*file+nameOff);
            const auto end=static_cast<std::size_t>(*file+strLen);
            auto limit=first;while(limit<end&&b[limit]!=0&&limit-first<=95)++limit;
            if(limit>=end||limit-first>95)return fail("Unterminated DT_NEEDED string");
            std::string name(reinterpret_cast<const char*>(b+first),limit-first);
            if(!safeName(name)||!unique.insert(name).second)return fail("Invalid or repeated DT_NEEDED name");
            report.neededNames.push_back(std::move(name));
        }
    }
    report.neededCount=static_cast<std::uint32_t>(report.neededNames.size());
    report.sceModuleCount=static_cast<std::uint32_t>(report.sceNeededIdentifiers.size());
    if(relaVA||relaLen||relaEnt){
        if(!relaVA||!relaLen||relaEnt!=24||relaLen>24*64||relaLen%24)
            return fail("Invalid DT_RELA table description");
        const auto f=locate(relaVA,relaLen);
        if(!f||!range(*f,relaLen,len))return fail("DT_RELA not file-backed");
        report.relativeTableEntries=static_cast<std::uint32_t>(relaLen/24);
        for(std::uint64_t i=0;i<relaLen;i+=24){
            const auto info=rd(b,static_cast<std::size_t>(*f+i+8),8);
            if((info&0xffffffff)!=8||(info>>32)!=0)report.hasUnsupportedRelocations=true;
        }
        report.relocationTableConsistent=true;
    }
    return report;
}
std::vector<std::uint8_t> makeSCE56Fixture(){
    auto b=makeSCE46Fixture();
    const std::size_t ph=64+2*56;
    wr(b,ph+8,0x3B0,8);wr(b,ph+16,0x30B0,8);
    wr(b,ph+32,0x70,8);wr(b,ph+40,0x70,8);
    // 0x398..0x3b0 holds a complete string table after the RELA record.
    const std::string name="libDemo.prx";
    std::copy(name.begin(),name.end(),b.begin()+0x398);
    wr(b,0x3B0,5,8);wr(b,0x3B8,0x3098,8); // DT_STRTAB
    wr(b,0x3C0,10,8);wr(b,0x3C8,0x18,8); // DT_STRSZ
    wr(b,0x3D0,1,8);wr(b,0x3D8,0,8); // DT_NEEDED at offset 0
    wr(b,0x3E0,7,8);wr(b,0x3E8,0x3080,8);
    wr(b,0x3F0,8,8);wr(b,0x3F8,24,8);
    wr(b,0x400,9,8);wr(b,0x408,24,8);
    // last 0x410..0x420 already zero (DT_NULL).
    return b;
}
} // namespace misaki
