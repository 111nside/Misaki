#include "GuestSCE46.hpp"
#include "GuestX64Backend.hpp"
#include <cassert>
#include <iostream>
using namespace misaki;
int main(){
    auto b=makeSCE46Fixture();GuestMemory mem;
    auto r=loadSCE46(b.data(),b.size(),0x100000,mem);
    assert(r && r->segments==2 && r->relocations==1 && r->imageBytes==0x200);
    assert(r->entry==0x101000 && r->relocationValue==0x103500);
    assert(mem.read64(0x103010)==0x103500 && !mem.write64(0x101000,0) && mem.isExecutable(0x101000));
    PortableX64Backend cpu(mem,r->entry);auto run=cpu.run(30);
    assert(run.stop==X64Stop::halted && run.rax()==42);
    auto invalid=b;invalid[0x388]=0xff;GuestMemory scratch=mem;
    assert(!loadSCE46(invalid.data(),invalid.size(),0x200000,scratch));
    assert(!scratch.read8(0x201000) && scratch.read64(0x103010)==0x103500);
    invalid=b;invalid[0x381]=0x10;invalid[0x380]=0x08;
    assert(!loadSCE46(invalid.data(),invalid.size(),0x200000,scratch));
    invalid=b;invalid[64+56+4]=7; // W+X PT_LOAD prohibited
    assert(!loadSCE46(invalid.data(),invalid.size(),0x200000,scratch));
    invalid=b;invalid[0x338]=0x00; // RELA virtual address cannot be resolved
    assert(!loadSCE46(invalid.data(),invalid.size(),0x200000,scratch));
    assert(!loadSCE46(nullptr,0,0,mem));
    assert(!loadSCE46(b.data(),b.size(),UINT64_MAX,mem));
    std::cout<<"PASS: compatibility 46 multi-segment SCE / ELF dynamic RELA staging\n";
}
