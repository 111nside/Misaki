#include "GuestSCEStaging.hpp"
#include "GuestPS4Formats.hpp"
#include "GuestX64Backend.hpp"
#include <cstdlib>
#include <iostream>
#include <limits>
#include <string>
using namespace misaki;
namespace {
unsigned count=0;
void check(bool v,const char*s){++count;if(!v){std::cerr<<"FAIL 41: "<<s<<'\n';std::exit(1);}}
void put(std::vector<std::uint8_t>&v,std::size_t p,std::uint64_t n,unsigned z){
    for(unsigned i=0;i<z;++i)v[p+i]=std::uint8_t(n>>(i*8));}
}
int main(){
    auto file=makeSCEStagingFixture();
    GuestMemory memory;
    std::string error;
    const auto good=stageSCEELFFixture(file.data(),file.size(),0x100000,
                                       {{0x1040,0x2000}},memory,&error);
    check(bool(good),"stage independent SCE ELF");
    check(error.empty(),"no success error");
    check(good->entry==0x101000,"biased entry");
    check(good->mappedSegments==1 && good->relativeRelocations==1,"mapped segment and patch");
    check(good->lastRelocationValue==0x102000,"relative patch value");
    check(memory.read64(0x101040)==0x102000,"patched guest memory");
    check(memory.isExecutable(0x101000),"RX code");
    check(!memory.write64(0x101040,10),"W^X sealed relocation target");
    PortableX64Backend cpu(memory,good->entry);
    const auto exec=cpu.run(20);
    check(exec.stop==X64Stop::halted && exec.rax()==42 && exec.stackRestored(),"guest execution unaffected");
    check(!stageSCEELFFixture(file.data(),file.size(),0x100000,{},memory),"overlap fail");
    check(memory.read64(0x101040)==0x102000,"overlap transactional");
    GuestMemory noPatch;
    check(bool(stageSCEELFFixture(file.data(),file.size(),0x200000,{},noPatch)),"zero-relocation fixture");
    check(noPatch.isExecutable(0x201000),"second bias works");
    auto encrypted=makePS4StructuralFixture(true);
    GuestMemory test;
    check(!stageSCEELFFixture(encrypted.data(),encrypted.size(),0x100000,{},test),"no encrypted SELF execution");
    auto clearSELF=makePS4StructuralFixture(false);
    check(!stageSCEELFFixture(clearSELF.data(),clearSELF.size(),0x100000,{},test),"no unwrapped SELF claims");
    auto bad=file;
    put(bad,68,7,4); // W+X segment
    check(!stageSCEELFFixture(bad.data(),bad.size(),0x100000,{},test),"reject W+X");
    bad=file;put(bad,72,0xffffffffffULL,8);
    check(!stageSCEELFFixture(bad.data(),bad.size(),0x100000,{},test),"reject segment outside file");
    bad=file;put(bad,96,0x101,8);
    check(!stageSCEELFFixture(bad.data(),bad.size(),0x100000,{},test),"reject filesz exceeds memsz");
    bad=file;put(bad,24,0x3000,8);
    check(!stageSCEELFFixture(bad.data(),bad.size(),0x100000,{},test),"reject entry outside RX");
    bad=file;put(bad,56,2,2);
    check(!stageSCEELFFixture(bad.data(),bad.size(),0x100000,{},test),"reject unsupported PHDR type");
    check(!stageSCEELFFixture(file.data(),file.size(),0x100000,
                               {{0x1040,0x2000},{0x1040,0x2001}},test),"duplicate relocation");
    check(!stageSCEELFFixture(file.data(),file.size(),0x100000,
                               {{0x1100,0}},test),"outside loaded segment");
    check(!stageSCEELFFixture(file.data(),file.size(),UINT64_MAX,{},test),"bias overflow");
    check(!stageSCEELFFixture(nullptr,file.size(),0,{},test),"null file");
    check(!test.isExecutable(0x101000),"failed operations leave no segment");
    check(!stageSCEELFFixture(file.data(),file.size(),0x100000,
                               {{0x1040,UINT64_MAX}},test),"relocation value overflow");
    check(!test.read64(0x101040),"no partial relocation mapping");
    std::cout<<"PASS: "<<count<<" milestone 41 ELF staging assertions\n";
}
