#include "../Include/GuestGCNLanes.hpp"
#include <vector>
namespace misaki { namespace {
constexpr std::uint64_t codeBase=0x100000, inputDesc=0x110000, outputDesc=0x110010;
constexpr std::uint64_t inputBase=0x400000, outputBase=0x500000;
void put32(std::vector<std::uint8_t> &v, std::uint32_t n) {
    for (unsigned k=0;k<4;++k) v.push_back(static_cast<std::uint8_t>(n>>(8*k)));
}
std::vector<std::uint8_t> bytesOf(const std::vector<std::uint32_t> &v) {
    std::vector<std::uint8_t> b;b.reserve(v.size()*4);
    for(auto n:v)put32(b,n);return b;
}
std::uint64_t hash(const std::array<std::uint32_t,64> &v) {
    std::uint64_t h=14695981039346656037ull;
    for (auto n:v)for(unsigned s=0;s<32;s+=8){h^=(n>>s)&255u;h*=1099511628211ull;}
    return h;
}
}
std::optional<GCNLane38Result> runGCNLaneMemoryDiagnostic(std::uint32_t mask) {
    if (!mask || (mask & ~15u)) return std::nullopt;
    GuestMemory memory;
    auto src=makeGCNBufferFixture();auto dst=src;
    dst[0]=static_cast<std::uint32_t>(outputBase);dst[1]=4u<<16;
    std::vector<std::uint32_t> descriptorWords(src.begin(),src.end());
    descriptorWords.insert(descriptorWords.end(),dst.begin(),dst.end());
    const auto descriptors=bytesOf(descriptorWords);
    GCNLane38Result r;
    r.activeMask=mask;
    if(!memory.map(inputDesc,descriptors.size(),permission::read|permission::write) ||
       !memory.writeBytes(inputDesc,descriptors.data(),descriptors.size()) ||
       !memory.protect(inputDesc,descriptors.size(),permission::read))return std::nullopt;
    r.descriptorsProtected=!memory.writeBytes(inputDesc,descriptors.data(),1);
    std::vector<std::uint8_t> source;
    for(unsigned i=0;i<64;++i){
        put32(source,i*3+10);
        for(unsigned k=0;k<12;++k)source.push_back(0);
        r.output[i]=0xB0000000u+i;
    }
    if(!memory.map(inputBase,source.size(),permission::read|permission::write) ||
       !memory.writeBytes(inputBase,source.data(),source.size()) ||
       !memory.protect(inputBase,source.size(),permission::read))return std::nullopt;
    r.inputProtected=!memory.writeBytes(inputBase,source.data(),1);
    std::vector<std::uint32_t> initial(r.output.begin(),r.output.end());
    const auto output=bytesOf(initial);
    if(!memory.map(outputBase,output.size(),permission::read|permission::write) ||
       !memory.writeBytes(outputBase,output.data(),output.size()))return std::nullopt;
    const auto indices=std::array<std::uint32_t,4>{{3,7,11,15}};
    // One shader binary per simulated lane; each is decoded from immutable
    // guest code. Changing the first SOPK immediate creates an independent VADDR.
    for(unsigned lane=0;lane<4;++lane){
        if(!(mask&(1u<<lane)))continue;
        auto code=makeFusedGCN30Fixture();
        code[0]=(code[0]&0xFFFF0000u)|indices[lane];
        const auto raw=bytesOf(code);
        const auto address=codeBase+std::uint64_t(lane)*0x100;
        if(!memory.map(address,raw.size(),permission::read|permission::write) ||
           !memory.writeBytes(address,raw.data(),raw.size()) ||
           !memory.protect(address,raw.size(),permission::read) ||
           memory.writeBytes(address,raw.data(),1))return std::nullopt;
        auto program=decodeGuestFusedGCN30(memory,address,code.size());
        if(!program)return std::nullopt;
        const auto executed=executeFusedGCN30(*program,memory,inputDesc,outputDesc,r.output);
        if(!executed || executed->index!=indices[lane] ||
           executed->stored != indices[lane]*3+10)return std::nullopt;
        ++r.executedLanes;
        r.instructions += executed->executed;
        r.indices[lane]=executed->index;
        r.loaded[lane]=executed->loaded;
        r.stored[lane]=executed->stored;
        r.output=executed->output;
    }
    r.codeProtected = true;
    r.checksum=hash(r.output);
    // Verify each output word against the mapped destination rather than
    // relying solely on the interpreter's returned report.
    for(unsigned i=0;i<64;++i){
        std::uint32_t read=0;
        for(unsigned s=0;s<32;s+=8){const auto b=memory.read8(outputBase+i*4+s/8);if(!b)return std::nullopt;read|=std::uint32_t(*b)<<s;}
        if(read!=r.output[i])return std::nullopt;
    }
    if(!r.descriptorsProtected||!r.inputProtected)return std::nullopt;
    return r;
}
}
