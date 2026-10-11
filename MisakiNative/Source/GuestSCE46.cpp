#include "../Include/GuestSCE46.hpp"
#include <algorithm>
#include <array>
#include <limits>
#include <set>
#include <utility>

namespace misaki { namespace {
constexpr std::uint64_t kMaxImageBytes=2*1024*1024;
constexpr std::uint64_t kMaxFile=8*1024*1024;
struct Segment { std::uint64_t file, vaddr, fileBytes, memBytes, address; std::uint8_t flags; };
void fail(std::string* e, const char* msg) { if(e)*e=msg; }
bool range(std::uint64_t p,std::uint64_t n,std::size_t length) {
    return p<=length && n<=length-p;
}
bool add(std::uint64_t a,std::uint64_t b,std::uint64_t &v) {
    if (b>UINT64_MAX-a) return false;
    v=a+b;return true;
}
std::uint64_t rd(const std::uint8_t *b,std::size_t at,unsigned n) {
    std::uint64_t v=0;for(unsigned k=0;k<n;++k)v|=std::uint64_t(b[at+k])<<(8*k);return v;
}
void put(std::vector<std::uint8_t>&b,std::size_t at,std::uint64_t n,unsigned size) {
    for(unsigned k=0;k<size;++k)b[at+k]=static_cast<std::uint8_t>(n>>(8*k));
}
// Locate an entire virtual address range in ONE file-backed PT_LOAD.
std::optional<std::uint64_t> toFile(const std::vector<Segment>&segs,
                                      std::uint64_t vaddr,std::uint64_t n) {
    for(const auto &s:segs) {
        if(vaddr>=s.vaddr && n<=s.fileBytes &&
           vaddr-s.vaddr<=s.fileBytes-n)return s.file+(vaddr-s.vaddr);
    }
    return std::nullopt;
}
} // anonymous namespace
std::optional<SCE46Image> loadSCE46(const std::uint8_t *b,std::size_t size,
                                    std::uint64_t bias,GuestMemory &memory,std::string *error) {
    if(error)error->clear();
    const auto reject=[&](const char *msg)->std::optional<SCE46Image>{fail(error,msg);return std::nullopt;};
    if(!b || size<120 || size>kMaxFile)return reject("Invalid SCE image size");
    const auto meta=inspectPS4Executable(b,size,error);
    if(!meta || meta->kind!=PS4ContainerKind::sceELF || meta->elfType!=0xFE10 ||
       meta->programHeaders<2 || meta->programHeaders>16)
        return reject("Only unwrapped, unencrypted SCE_DYNEXEC ELF64 with segments supported");
    const std::uint64_t phoff=rd(b,32,8);
    std::vector<Segment> segs;
    std::uint64_t bytes=0,dynOffset=0,dynSize=0,dynVA=0;
    bool seenDynamic=false;
    for(std::uint32_t i=0;i<meta->programHeaders;++i) {
        if(phoff>UINT64_MAX-std::uint64_t(i)*56 ||
           !range(phoff+std::uint64_t(i)*56,56,size))return reject("Truncated program header");
        const std::size_t at=static_cast<std::size_t>(phoff+std::uint64_t(i)*56);
        const auto type=rd(b,at,4),flags=rd(b,at+4,4),file=rd(b,at+8,8),vaddr=rd(b,at+16,8);
        const auto filesz=rd(b,at+32,8),memsz=rd(b,at+40,8);
        if(type==2) { // ordinary PT_DYNAMIC in file; never execute its pointers
            if(seenDynamic || !range(file,filesz,size) || filesz==0 ||
               filesz>1024 || (filesz%16))return reject("Unsupported PT_DYNAMIC table");
            seenDynamic=true;dynOffset=file;dynSize=filesz;dynVA=vaddr;
        } else if(type==1) {
            std::uint64_t addr;
            if((flags & ~7u) || (flags&3u)==3u || (flags&4u)==0 ||
               memsz==0 || memsz>1024*1024 || filesz>memsz ||
               !range(file,filesz,size) || bytes>kMaxImageBytes-memsz ||
               !add(vaddr,bias,addr) || addr>UINT64_MAX-memsz)
                return reject("Unsupported/overlapping segment candidate");
            bytes+=memsz;
            const auto perm=static_cast<std::uint8_t>(((flags&4)?permission::read:0) |
                 ((flags&2)?permission::write:0) | ((flags&1)?permission::execute:0));
            segs.push_back({file,vaddr,filesz,memsz,addr,perm});
        } else return reject("Unsupported program-header kind");
    }
    if(segs.size()<2 || !seenDynamic)return reject("Expected multiple PT_LOAD and PT_DYNAMIC");
    const auto mappedDynamic=toFile(segs,dynVA,dynSize);
    if(!mappedDynamic || *mappedDynamic!=dynOffset)return reject("PT_DYNAMIC is not in the file-backed loaded image");
    GuestMemory scratch=memory;
    for(const auto&s:segs) {
        if(!scratch.map(s.address,static_cast<std::size_t>(s.memBytes),permission::read|permission::write) ||
           (s.fileBytes && !scratch.writeBytes(s.address,b+s.file,static_cast<std::size_t>(s.fileBytes))))
           return reject("Segment overlap or invalid mapping");
    }
    std::uint64_t relaVA=0,relaSize=0,relaEntry=0;
    bool gotRela=false,gotSize=false,gotEntry=false,terminated=false;
    for(std::uint64_t i=0;i<dynSize;i+=16) {
        const auto at=static_cast<std::size_t>(dynOffset+i);
        const auto tag=rd(b,at,8),value=rd(b,at+8,8);
        if(tag==0){terminated=true;break;}
        if(tag==7 && !gotRela){relaVA=value;gotRela=true;}
        else if(tag==8 && !gotSize){relaSize=value;gotSize=true;}
        else if(tag==9 && !gotEntry){relaEntry=value;gotEntry=true;}
        else return reject("Unsupported or repeated dynamic tag");
    }
    if(!terminated || !gotRela || !gotSize || !gotEntry || relaEntry!=24 ||
       relaSize==0 || relaSize>24*64 || relaSize%24)
        return reject("Invalid DT_RELA description");
    const auto relaFile=toFile(segs,relaVA,relaSize);
    if(!relaFile || !range(*relaFile,relaSize,size))return reject("Relocations outside initialized PT_LOAD");
    SCE46Image out;out.segments=static_cast<std::uint32_t>(segs.size());out.imageBytes=bytes;
    std::set<std::uint64_t> positions;
    for(std::uint64_t i=0;i<relaSize;i+=24) {
        const auto at=static_cast<std::size_t>(*relaFile+i);
        const auto targetVA=rd(b,at,8),info=rd(b,at+8,8),valueVA=rd(b,at+16,8);
        std::uint64_t target,value;
        if(info!=8 || !add(bias,targetVA,target) || !add(bias,valueVA,value) ||
           !positions.insert(target).second)return reject("Unsupported or duplicate RELA relocation");
        bool inWritable=false;
        for(const auto&s:segs) {
            if((s.flags&permission::write) && s.memBytes>=8 &&
               target>=s.address && target-s.address<=s.memBytes-8)
                inWritable=true;
        }
        if(!inWritable || !scratch.write64(target,value))
            return reject("Relocation target is not a writable image segment");
        ++out.relocations;out.relocationValue=value;
    }
    for(const auto&s:segs) {
        if(!scratch.protect(s.address,static_cast<std::size_t>(s.memBytes),s.flags))
            return reject("Cannot seal loaded segment permissions");
    }
    if(!add(meta->entry,bias,out.entry) || !scratch.isExecutable(out.entry))
        return reject("Entry point not executable");
    memory=std::move(scratch);
    return out;
}
std::vector<std::uint8_t> makeSCE46Fixture() {
    std::vector<std::uint8_t> b(0x500,0);
    b[0]=0x7f;b[1]='E';b[2]='L';b[3]='F';b[4]=2;b[5]=1;b[6]=1;
    put(b,16,0xfe10,2);put(b,18,62,2);put(b,20,1,4);
    put(b,24,0x1000,8);put(b,32,64,8);put(b,52,64,2);put(b,54,56,2);put(b,56,3,2);
    auto ph=[&](unsigned i,unsigned type,unsigned flags,std::uint64_t file,
                std::uint64_t virtualAddress,std::uint64_t filesz,std::uint64_t memsz){
        const std::size_t p=64+i*56;put(b,p,type,4);put(b,p+4,flags,4);
        put(b,p+8,file,8);put(b,p+16,virtualAddress,8);
        put(b,p+32,filesz,8);put(b,p+40,memsz,8);put(b,p+48,0x1000,8);
    };
    ph(0,1,5,0x200,0x1000,0x40,0x80); // RX
    ph(1,1,6,0x300,0x3000,0x120,0x180); // RW
    ph(2,2,6,0x330,0x3030,0x40,0x40); // dynamic lives inside RW
    const std::array<std::uint8_t,12> code={{0x48,0xB8,42,0,0,0,0,0,0,0,0xF4}};
    std::copy(code.begin(),code.end(),b.begin()+0x200);
    put(b,0x330,7,8);put(b,0x338,0x3080,8);
    put(b,0x340,8,8);put(b,0x348,24,8);
    put(b,0x350,9,8);put(b,0x358,24,8);
    put(b,0x380,0x3010,8);put(b,0x388,8,8);put(b,0x390,0x3500,8);
    return b;
}
} // namespace misaki
