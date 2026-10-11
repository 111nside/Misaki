#include "GuestSCEBinding.hpp"
#include <cstdlib>
#include <iostream>
using namespace misaki;
namespace {
unsigned count=0;
void check(bool v,const char*s){++count;if(!v){std::cerr<<"FAIL 42: "<<s<<'\n';std::exit(1);}}
}
int main(){
    GuestMemory m;
    check(installPS4SyntheticImportFunctions(m),"install independent RX functions");
    check(m.map(0x150000,64,permission::read|permission::write),"map GOT");
    auto catalog=makePS4SyntheticImportCatalog(m);
    check(bool(catalog),"catalog available");
    auto slots=std::vector<SCEImportSlot>{{makePS4SyntheticKey(0),0x150000},
                                          {makePS4SyntheticKey(1),0x150008}};
    auto bound=bindSCEImportSlots(*catalog,slots,0x150000,64,m);
    check(bool(bound),"all imports bound");
    check(bound->bindings==2 && bound->firstAddress==0x9000 && bound->sealedReadOnly,"report");
    check(m.read64(0x150000)==0x9000 && m.read64(0x150008)==0x9010,"resolved targets");
    check(!m.write64(0x150000,0xDEAD),"read-only GOT after binding");
    check(m.read64(0x150000)==0x9000,"sealed target unchanged");
    check(!bindSCEImportSlots(*catalog,slots,0x150000,64,m),"second binding rejected");
    GuestMemory n;
    check(installPS4SyntheticImportFunctions(n),"fresh functions");
    check(n.map(0x150000,64,permission::read|permission::write),"fresh GOT");
    auto invalid=slots;
    invalid[1].symbol.major=99;
    check(!bindSCEImportSlots(*catalog,invalid,0x150000,64,n),"strict SCE version");
    check(n.read64(0x150000)==0,"failed binding did not patch first slot");
    invalid=slots; invalid[1].address=invalid[0].address;
    check(!bindSCEImportSlots(*catalog,invalid,0x150000,64,n),"duplicate slot");
    invalid=slots; invalid[1].address=0x150004;
    check(!bindSCEImportSlots(*catalog,invalid,0x150000,64,n),"misaligned slot");
    invalid=slots; invalid[1].address=0x150040;
    check(!bindSCEImportSlots(*catalog,invalid,0x150000,64,n),"out-of-bounds slot");
    invalid=slots; invalid[1].address=0x150038;
    check(bool(bindSCEImportSlots(*catalog,invalid,0x150000,64,n)),"last valid slot");
    check(n.read64(0x150038)==0x9010,"last valid bound target");
    GuestMemory p;
    check(installPS4SyntheticImportFunctions(p),"fresh functions 3");
    check(p.map(0x150000,64,permission::read|permission::write),"fresh GOT 3");
    auto c=makePS4SyntheticImportCatalog(p);
    check(!bindSCEImportSlots(*c,slots,0x150000,56,p),"seal requires whole region");
    check(p.read64(0x150000)==0,"failed sealing transactional");
    check(!bindSCEImportSlots(*c,{},0x150000,64,p),"empty import set");
    check(!bindSCEImportSlots(*c,slots,UINT64_MAX-1,64,p),"region overflow");
    check(p.read64(0x150000)==0,"invalid requests unchanged");
    std::cout<<"PASS: "<<count<<" milestone 42 import-link assertions\n";
}
