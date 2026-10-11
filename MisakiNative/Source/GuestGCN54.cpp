#include "../Include/GuestGCN54.hpp"
#include "../Include/GuestPM4Indirect.hpp"
#include <vector>
namespace misaki { namespace {
void put32(std::vector<std::uint8_t> &v,std::uint32_t word) {
    for(unsigned shift=0;shift<32;shift+=8)v.push_back(static_cast<std::uint8_t>(word>>shift));
}
std::vector<std::uint8_t> encode(const std::vector<std::uint32_t> &words){
    std::vector<std::uint8_t> data;data.reserve(words.size()*4);
    for(auto word:words)put32(data,word);
    return data;
}
std::uint64_t hash(const std::array<std::uint32_t,64> &output){
    std::uint64_t h=14695981039346656037ull;
    for(auto word:output)for(unsigned shift=0;shift<32;shift+=8){
        h^=(word>>shift)&255u;h*=1099511628211ull;
    }
    return h;
}
} // namespace
std::optional<GCN54Report> runGCN54(std::uint32_t mask,std::string *error){
    if(error)error->clear();
    const auto fail=[&](const char *why)->std::optional<GCN54Report>{
        if(error)*error=why;return std::nullopt;
    };
    if(!mask || (mask & ~15u))return fail("Only lanes 0..3 are modeled");
    GuestMemory memory;
    const auto p=makePM4IndirectFixture();
    if(!installPM4IndirectFixture(memory,p))return fail("PM4 guest fixture failed");
    const auto pm4=analyzePM4DrawStates(memory,p.root,p.rootWords.size(),error);
    if(!pm4 || pm4->draws.size()!=1 ||
       pm4->draws[0].status!=PM4DrawStatus::metadataReady ||
       pm4->draws[0].pixelShaderAddress!=0x100000)
        return fail("PM4 draw state not ready for the simulated shader");
    GCN54Report report;
    report.pm4Packets=pm4->packets;report.draws=static_cast<std::uint32_t>(pm4->draws.size());
    report.colorTarget=pm4->draws[0].colorAddress;
    report.shaderAddress=pm4->draws[0].pixelShaderAddress;report.laneMask=mask;
    constexpr std::uint64_t inputAddress=0x400000,outputAddress=0x500000;
    constexpr std::uint64_t descriptorAddress=0x110000;
    auto in=makeGCNBufferFixture(),out=in;
    out[0]=static_cast<std::uint32_t>(outputAddress);
    out[1]=4u<<16;
    std::vector<std::uint32_t> descriptorWords(in.begin(),in.end());
    descriptorWords.insert(descriptorWords.end(),out.begin(),out.end());
    const auto descriptorData=encode(descriptorWords);
    if(!memory.map(descriptorAddress,descriptorData.size(),permission::read|permission::write) ||
       !memory.writeBytes(descriptorAddress,descriptorData.data(),descriptorData.size()) ||
       !memory.protect(descriptorAddress,descriptorData.size(),permission::read))
        return fail("Descriptor setup failed");
    report.descriptorsReadOnly=!memory.writeBytes(descriptorAddress,descriptorData.data(),1);
    std::vector<std::uint8_t> inputData;
    for(unsigned i=0;i<64;++i){
        put32(inputData,i*3+10);
        for(unsigned j=0;j<12;++j)inputData.push_back(0);
        report.output[i]=0xB0000000u+i;
    }
    if(!memory.map(inputAddress,inputData.size(),permission::read|permission::write) ||
       !memory.writeBytes(inputAddress,inputData.data(),inputData.size()) ||
       !memory.protect(inputAddress,inputData.size(),permission::read))
        return fail("Input buffer setup failed");
    report.inputReadOnly=!memory.writeBytes(inputAddress,inputData.data(),1);
    std::vector<std::uint8_t> outputData;
    for(auto word:report.output)put32(outputData,word);
    if(!memory.map(outputAddress,outputData.size(),permission::read|permission::write) ||
       !memory.writeBytes(outputAddress,outputData.data(),outputData.size()))
        return fail("Output buffer setup failed");
    constexpr std::array<std::uint32_t,4> indexes{{3,7,11,15}};
    report.shaderReadOnly=true;
    for(unsigned lane=0;lane<4;++lane){
        if(!(mask & (1u<<lane)))continue;
        auto fixture=makeFusedGCN30Fixture();
        fixture[0]=(fixture[0]&0xFFFF0000u)|indexes[lane];
        const auto bytes=encode(fixture);
        const auto address=report.shaderAddress+std::uint64_t(lane)*0x100;
        if(!memory.map(address,bytes.size(),permission::read|permission::write) ||
           !memory.writeBytes(address,bytes.data(),bytes.size()) ||
           !memory.protect(address,bytes.size(),permission::read) ||
           memory.writeBytes(address,bytes.data(),1))
            return fail("Shader code protection failed");
        const auto program=decodeGuestFusedGCN30(memory,address,fixture.size());
        if(!program)return fail("Unsupported GCN shader program");
        const auto run=executeFusedGCN30(*program,memory,descriptorAddress,
                                         descriptorAddress+16,report.output);
        if(!run || run->index!=indexes[lane] || run->stored!=indexes[lane]*3+10)
            return fail("Decoded GCN MUBUF program failed");
        report.output=run->output;
        report.indices[lane]=run->index;report.loaded[lane]=run->loaded;
        ++report.lanes;report.instructions+=run->executed;
    }
    for(unsigned i=0;i<64;++i){
        std::uint32_t read=0;
        for(unsigned k=0;k<4;++k){
            const auto b=memory.read8(outputAddress+i*4+k);
            if(!b)return fail("Guest output read failed");
            read|=std::uint32_t(*b)<<(8*k);
        }
        if(read!=report.output[i])return fail("GCN guest output mismatch");
    }
    if(!report.shaderReadOnly || !report.inputReadOnly || !report.descriptorsReadOnly)
        return fail("Read-only GCN resources writable");
    report.outputHash=hash(report.output);
    return report;
}
} // namespace misaki
