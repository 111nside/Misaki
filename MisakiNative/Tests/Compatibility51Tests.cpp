#include "GuestSCE51.hpp"
#include "GuestSCE46.hpp"
#include <iostream>
#include <cstdlib>
#include <cstdint>
using namespace misaki;
static int checks=0;
static void check(bool ok,const char *why){++checks;if(!ok){std::cerr<<"FAIL M51: "<<why<<'\n';std::exit(1);}}
static void put(std::vector<std::uint8_t> &bytes,std::size_t at,std::uint64_t v,unsigned count){
    for(unsigned k=0;k<count;++k)bytes.at(at+k)=static_cast<std::uint8_t>(v>>(8*k));
}
int main(){
    auto elf=makeSCE46Fixture();
    auto a=inspectSCE51(elf.data(),elf.size());
    check(bool(a),"valid unwrapped SCE ELF");
    check(a->container==PS4ContainerKind::sceELF,"unwrapped classification");
    check(a->disposition==SCE51Disposition::unencryptedFixtureOnly,"fixture-only disposition");
    check(a->elfType==0xFE10 && a->entry==0x1000,"SCE type and entry");
    check(a->loadSegments==2 && a->dynamicSegments==1,"load and dynamic headers");
    check(a->executableSegments==1,"executable segment metadata");
    check(a->segments.size()==3 && a->segments[1].virtualAddress==0x3000,"segment fields");
    check(a->structuralOnly && !a->firmwareExecutable,"never assert firmware execution");
    auto wrapper=makePS4StructuralFixture(true);
    auto b=inspectSCE51(wrapper.data(),wrapper.size());
    check(bool(b),"encrypted SELF structurally inspectable");
    check(b->container==PS4ContainerKind::self,"SELF recognized");
    check(b->disposition==SCE51Disposition::encryptedPayload,"encrypted SELF refused");
    check(b->encryptedSegments==1 && !b->firmwareExecutable,"no decrypt or execution");
    auto compressed=makePS4StructuralFixture(false);
    compressed.at(32)=8;
    auto c=inspectSCE51(compressed.data(),compressed.size());
    check(c && c->compressedSegments==1 &&
          c->disposition==SCE51Disposition::compressedPayload,"compressed SELF refused");
    auto unsupported=elf;
    put(unsupported,64+2*56,0xDEADu,4);
    auto d=inspectSCE51(unsupported.data(),unsupported.size());
    check(d && d->disposition==SCE51Disposition::unsupportedLayout,"unknown PT kind tracked");
    auto outside=elf;
    put(outside,64+8,0x100000,8);
    check(!inspectSCE51(outside.data(),outside.size()),"unwrapped PT_LOAD file range bounded");
    auto memoryRange=elf;
    put(memoryRange,64+16,UINT64_MAX,8);
    check(!inspectSCE51(memoryRange.data(),memoryRange.size()),"virtual overflow blocked");
    auto badFlags=elf;
    put(badFlags,64+4,7,4);
    check(!inspectSCE51(badFlags.data(),badFlags.size()),"W+X refused");
    auto missingExec=elf;
    put(missingExec,64+4,4,4);
    auto e=inspectSCE51(missingExec.data(),missingExec.size());
    check(e && e->disposition==SCE51Disposition::unsupportedLayout,"no executable segment");
    check(!inspectSCE51(elf.data(),63),"short ELF refused");
    check(!inspectSCE51(nullptr,elf.size()),"null input refused");
    auto phOverflow=elf;
    put(phOverflow,32,UINT64_MAX,8);
    check(!inspectSCE51(phOverflow.data(),phOverflow.size()),"phoff addition bounded");
    std::cout<<"PASS: "<<checks<<" SCE51 checks\n";
}
