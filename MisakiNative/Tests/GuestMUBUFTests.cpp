#include "GuestMUBUF.hpp"
#include "MisakiMUBUFBridge.h"

#include <array>
#include <cstdlib>
#include <iostream>
#include <limits>
#include <string>
#include <vector>

namespace {
int checks = 0;
void check(bool yes, const char *message) {
    ++checks;
    if (!yes) { std::cerr << "FAIL: " << message << "\n"; std::exit(1); }
}
void put32(std::uint8_t *dst, std::uint32_t word) {
    for (unsigned i = 0; i < 4; ++i) dst[i] = static_cast<std::uint8_t>(word >> (8 * i));
}
std::vector<std::uint8_t> bytes(const std::uint32_t *words, std::size_t size) {
    std::vector<std::uint8_t> result(size * 4);
    for (std::size_t i = 0; i < size; ++i) put32(result.data() + i * 4, words[i]);
    return result;
}
struct Setup {
    misaki::GuestMemory mem;
    std::array<uint32_t,64> initial{};
    bool ready = false;
    Setup() {
        using namespace misaki;
        auto descIn = makeGCNBufferFixture();
        auto descOut = descIn;
        descOut[0] = 0x500000u;
        descOut[1] = 4u << 16;
        auto bIn = bytes(descIn.data(), descIn.size());
        auto bOut = bytes(descOut.data(), descOut.size());
        std::array<uint8_t,1024> source{};
        std::array<uint8_t,256> output{};
        for (unsigned i=0; i<64; ++i) {
            put32(source.data()+i*16, i*3+10);
            initial[i]=0xA0000000u+i;
            put32(output.data()+i*4, initial[i]);
        }
        ready = mem.map(0x110000,32,permission::read | permission::write) &&
                mem.writeBytes(0x110000,bIn.data(),bIn.size()) &&
                mem.writeBytes(0x110010,bOut.data(),bOut.size()) &&
                mem.protect(0x110000,32,permission::read) &&
                mem.map(0x400000,1024,permission::read | permission::write) &&
                mem.writeBytes(0x400000,source.data(),source.size()) &&
                mem.protect(0x400000,1024,permission::read) &&
                mem.map(0x500000,256,permission::read | permission::write) &&
                mem.writeBytes(0x500000,output.data(),output.size());
    }
};
}
int main() {
    using namespace misaki;
    std::string error;
    const auto code = makeMUBUF29Fixture();
    check(code.size()==4,"two 64-bit GCN MUBUF instructions");
    check((code[0]&0xFC000000u)==0xE0000000u,"AMD 6-bit encoding tag");
    auto trace = decodeMUBUFWords(code.data(),code.size(),&error);
    check(trace.has_value(),"decode real-format GCN1.1 MUBUF");
    check(error.empty(),"success has no error");
    check(trace->loads==1 && trace->stores==1,"one load / one store");
    check(trace->instructions.size()==2,"two instructions");
    check(trace->checksum!=0,"nonzero trace checksum");
    check(decodeMUBUFWords(code.data(),code.size())->checksum==trace->checksum,
          "stable instruction checksum");
    check(trace->instructions[0].opcode==MUBUFOp::loadDword &&
          trace->instructions[1].opcode==MUBUFOp::storeDword,"AMD opcodes 12 and 28");
    check(trace->instructions[0].srsrc==0 && trace->instructions[1].srsrc==4,
          "resource descriptor SGPR tuples");
    check(trace->instructions[0].vaddr==4 && trace->instructions[0].vdata==8,
          "encoded VGPR fields");
    check(trace->instructions[1].vaddr==4 && trace->instructions[1].vdata==8,
          "store VGPR fields");
    check(trace->instructions[0].idxen && trace->instructions[1].idxen,
          "IDXEN addressing");
    check(trace->instructions[0].soffset==128 && trace->instructions[1].soffset==128,
          "zero inline SOFFSET");
    check(trace->instructions[0].wordOffset==0 && trace->instructions[1].wordOffset==2,
          "stream byte ordering");
    check(!decodeMUBUFWords(nullptr,4),"reject null program");
    check(!decodeMUBUFWords(code.data(),0),"reject empty program");
    check(!decodeMUBUFWords(code.data(),3),"reject half instruction");
    check(!decodeMUBUFWords(code.data(),130),"reject excessive instruction count");
    for (unsigned b=0;b<32;++b) {
        auto mutated=code;mutated[0]^=1u<<b;
        auto parsed=decodeMUBUFWords(mutated.data(),mutated.size());
        if (b>=26 || b==25 || b==17) check(!parsed,"reject wrong tag/reserved bit");
        else if (b>=18 && b<=24) {
            if (((mutated[0]>>18)&127u)!=12 && ((mutated[0]>>18)&127u)!=28)
                check(!parsed,"reject unsupported 7-bit opcode");
            else check(bool(parsed),"supported opcode remains decodable");
        } else check(bool(parsed),"operand and flag metadata remains parsable");
    }
    for (unsigned b=0;b<32;++b) {
        auto mutated=code;mutated[1]^=1u<<b;
        auto parsed=decodeMUBUFWords(mutated.data(),mutated.size());
        if (b==21) check(!parsed,"reject reserved second-word bit");
        else check(bool(parsed),"preserve variable operand and flag fields");
    }
    for (unsigned op=0;op<128;++op) {
        auto modified=code;
        modified[0]=(modified[0]&~(127u<<18))|(op<<18);
        const auto parsed=decodeMUBUFWords(modified.data(),modified.size());
        check(bool(parsed)==(op==12 || op==28),"allowlisted MUBUF opcodes only");
    }
    Setup setup;
    check(setup.ready,"test guest memory mapped with read/write isolation");
    const auto bytecode=bytes(code.data(),code.size());
    check(setup.mem.map(0x120000,bytecode.size(),permission::read|permission::write),
          "allocate guest instruction words");
    check(setup.mem.writeBytes(0x120000,bytecode.data(),bytecode.size()),
          "write guest instructions");
    check(setup.mem.protect(0x120000,bytecode.size(),permission::read),
          "protect guest instruction stream");
    auto guestTrace=decodeGuestMUBUF(setup.mem,0x120000,code.size());
    check(guestTrace && guestTrace->checksum==trace->checksum,"guest decode parity");
    check(!decodeGuestMUBUF(setup.mem,0x120004,4),"reject misaligned instruction address");
    check(!decodeGuestMUBUF(setup.mem,0x110020,4),"reject unmapped guest code");
    check(!decodeGuestMUBUF(setup.mem,UINT64_MAX-7,4),"reject overflowing guest range");
    check(!decodeGuestMUBUF(setup.mem,0x120000,6),"reject truncated guest memory");
    auto reference=executeMUBUF29(*guestTrace,setup.mem,0x110000,0x110010,7,setup.initial);
    check(reference.has_value(),"decode + execute bounded guest MUBUF");
    check(reference->index==7 && reference->loaded==31 && reference->stored==31,
          "record 7 yields first DWORD 31");
    for (unsigned i=0;i<64;++i) {
        check(reference->output[i]==(i==7 ? 31u : setup.initial[i]),
              "no unrelated output record mutation");
        std::uint32_t raw=0;
        for (unsigned k=0;k<4;++k)
            raw|=std::uint32_t(setup.mem.read8(0x500000 + i*4 + k).value_or(0))<<(k*8);
        check(raw==reference->output[i],"guest buffer readback parity");
    }
    check(!executeMUBUF29(*guestTrace,setup.mem,0x110000,0x110010,64,setup.initial),
          "reject out-of-range index");
    check(!executeMUBUF29(*guestTrace,setup.mem,0x110000,0x110000,7,setup.initial),
          "reject aliased descriptor slot");
    check(!executeMUBUF29(*guestTrace,setup.mem,0x110000,0xDEAD0000,7,setup.initial),
          "reject unreadable descriptor");
    {
        Setup unwritable;
        check(unwritable.ready, "new guest mapping for readonly-write rejection");
        check(unwritable.mem.protect(0x500000, 256, permission::read),
              "make output resource readonly");
        check(!executeMUBUF29(*trace, unwritable.mem, 0x110000, 0x110010,
                              7, unwritable.initial),
              "reject readonly output without partial writes");
        check(unwritable.mem.read8(0x500000+7*4) ==
                  std::optional<uint8_t>(static_cast<uint8_t>(unwritable.initial[7])),
              "readonly destination unchanged");
    }
    {
        Setup unreadable;
        check(unreadable.ready, "new guest mapping for unreadable source rejection");
        check(unreadable.mem.protect(0x400000, 1024, permission::write),
              "remove input resource read permission");
        check(!executeMUBUF29(*trace, unreadable.mem, 0x110000, 0x110010,
                              7, unreadable.initial),
              "reject input read fault");
        check(unreadable.mem.read8(0x500000+7*4) ==
                  std::optional<uint8_t>(static_cast<uint8_t>(unreadable.initial[7])),
              "destination untouched after source read fault");
    }
    auto sample=trace.value();
    for (unsigned flag=12;flag<=16;++flag) {
        sample=trace.value();
        const auto altered=code[0]^(1u<<flag);
        auto mutated=code;mutated[0]=altered;
        const auto t=decodeMUBUFWords(mutated.data(),mutated.size());
        check(bool(t),"unsupported addressing metadata still decodes");
        check(!executeMUBUF29(*t,setup.mem,0x110000,0x110010,7,setup.initial),
              "unsupported flag refuses execution");
    }
    for (unsigned flag: {22u,23u}) {
        auto mutated=code;mutated[1]|=1u<<flag;
        const auto t=decodeMUBUFWords(mutated.data(),mutated.size());
        check(bool(t),"SLC/TFE flags decode");
        check(!executeMUBUF29(*t,setup.mem,0x110000,0x110010,7,setup.initial),
              "SLC/TFE not silently executed");
    }
    {
        auto mutated=code;mutated[1]=(mutated[1]&0x00FFFFFFu)|(0u<<24);
        const auto t=decodeMUBUFWords(mutated.data(),mutated.size());
        check(bool(t) && !executeMUBUF29(*t,setup.mem,0x110000,0x110010,7,setup.initial),
              "nonzero/unknown SOFFSET execution rejected");
    }
    auto in=decodeGuestGCNBuffer(setup.mem,0x110000);
    auto out=decodeGuestGCNBuffer(setup.mem,0x110010);
    check(in && in->strideBytes==16 && in->records==64,"input resource descriptor");
    check(out && out->strideBytes==4 && out->records==64,"output resource descriptor");
    const auto msl=translateMUBUF29ToMetal(*trace,*in,*out);
    check(msl && msl->find("kernel void misaki_mubuf29")!=std::string::npos,
          "source generated from decoded MUBUF");
    check(msl->find("guest_v8")!=std::string::npos &&
          msl->find("guest_v4 * 4u")!=std::string::npos,"decoded operands in MSL");
    check(!translateMUBUF29ToMetal(*trace,*in,*in),"reject aliasing in MSL translation");
    const auto diagnostic=runMUBUF29Diagnostic();
    check(diagnostic && diagnostic->decoded.checksum==trace->checksum,"integrated diagnostic");
    check(diagnostic->recordIndex==7 && diagnostic->loaded==31,
          "diagnostic result is real descriptor memory content");
    check(diagnostic->codeReadOnly && diagnostic->descriptorsReadOnly &&
          diagnostic->sourceReadOnly && diagnostic->outputWritable,"permissions tested");
    check(diagnostic->initial[7]!=diagnostic->expected[7],"exactly one output differs");
    for (unsigned i=0;i<64;++i)
        check(diagnostic->expected[i]==(i==7 ? 31u : diagnostic->initial[i]),
              "independent program output comparison");
    check(diagnostic->metalSource.size()<4096 && diagnostic->shaderSourceHash!=0,
          "bounded deterministic Metal generation");
    const auto same=runMUBUF29Diagnostic();
    check(same && same->resultHash==diagnostic->resultHash &&
          same->shaderSourceHash==diagnostic->shaderSourceHash,"diagnostic reproducible");
    {
        MisakiMUBUF29Report report{};
        char source[4096]{};
        uint8_t input[1024]{};
        uint32_t initial[64]{},expected[64]{};
        check(misaki_mubuf29_plan(source,4096,input,1024,initial,64,expected,64,&report)==0,
              "M29 C ABI produces full GPU plan");
        check(report.abi_version==1 && report.decoded_instructions==2 &&
              report.load_opcode==12 && report.store_opcode==28,"C ABI decoded op metadata");
        check(report.first_vaddr==4 && report.first_vdata==8 &&
              report.input_srsrc==0 && report.output_srsrc==4,"C ABI operands");
        check(report.record_index==7 && report.loaded_word==31 && report.stored_word==31,
              "C ABI result values");
        check(report.input_base==0x400000 && report.output_base==0x500000,
              "C ABI resource addresses");
        check(report.source_bytes==diagnostic->metalSource.size()+1 &&
              report.instruction_checksum==trace->checksum,"C ABI byte and hash totals");
        check(report.code_read_only && report.descriptors_read_only &&
              report.input_read_only && report.output_writable,"C ABI safety properties");
        check(expected[7]==31 && initial[7]!=31 && expected[6]==initial[6],
              "C output scene parity");
        source[0]='Z';input[0]=0xCD;initial[0]=0xBADDCAFEu;expected[0]=0xBADDCAFEu;
        check(misaki_mubuf29_plan(source,1,input,1024,initial,64,expected,64,&report)==-3,
              "short source capacity rejected");
        check(source[0]=='Z' && input[0]==0xCD && initial[0]==0xBADDCAFEu &&
              expected[0]==0xBADDCAFEu && report.abi_version==0,
              "failed C bridge does not partially overwrite");
        check(misaki_mubuf29_plan(source,4096,input,1023,initial,64,expected,64,&report)==-3,
              "short source bytes rejected");
        check(misaki_mubuf29_plan(source,4096,input,1024,initial,63,expected,64,&report)==-3,
              "short initial array rejected");
        check(misaki_mubuf29_plan(source,4096,input,1024,initial,64,expected,63,&report)==-3,
              "short expected array rejected");
        check(misaki_mubuf29_plan(source,4096,input,1024,initial,64,expected,64,nullptr)==-1,
              "null report rejected");
    }
    std::uint32_t seed=0x9134B21Du;
    for (int iteration=0;iteration<500;++iteration) {
        std::vector<uint32_t> random(2 * (1+(iteration%12)));
        for (auto &item:random) {
            seed ^= seed << 13;seed ^= seed >> 17;seed ^= seed << 5;
            item = seed;
        }
        const auto parsed=decodeMUBUFWords(random.data(),random.size());
        check(!parsed || parsed->instructions.size() <= random.size()/2,
              "random fuzz stream stays bounded");
    }
    std::cout << "PASS: " << checks << " GCN MUBUF assertions\n";
}
