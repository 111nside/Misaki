#include "GuestSCE47.hpp"
#include <cassert>
#include <iostream>
using namespace misaki;
int main(){
    GuestMemory mem;
    assert(installPS4SyntheticImportFunctions(mem));
    for(auto address:{0x150000u,0x151000u})assert(mem.map(address,32,permission::read|permission::write));
    auto catalog=makePS4SyntheticImportCatalog(mem);assert(catalog);
    auto modules=makeSCE47Modules();
    auto result=bindSCE47Graph(modules,*catalog,mem);
    assert(result && result->imports==2 && result->modules==2 && result->sealed);
    assert(result->loadOrder[0]=="libSceDemoCore" && result->loadOrder[1]=="libSceDemoGraphics");
    assert(mem.read64(0x150000)==0x9000 && mem.read64(0x151000)==0x9010);
    assert(!mem.write64(0x150000,7) && !mem.write64(0x151000,7));
    auto bad=modules;bad[0].dependencies={"missing"};
    assert(!bindSCE47Graph(bad,*catalog,mem));
    bad=modules;bad[1].dependencies={bad[0].name};
    assert(!bindSCE47Graph(bad,*catalog,mem));
    bad=modules;bad[0].slots[0].symbol.minor=1;
    assert(!bindSCE47Graph(bad,*catalog,mem));
    bad=modules;bad[0].slots[0].symbol.nid="FakeNid0000";
    assert(!bindSCE47Graph(bad,*catalog,mem));
    // Transaction rollback: failed second module must not seal first module.
    GuestMemory fresh;assert(installPS4SyntheticImportFunctions(fresh));
    for(auto address:{0x150000u,0x151000u})assert(fresh.map(address,32,permission::read|permission::write));
    bad=modules;bad[0].slots[0].address=0x151008; // belongs to graphics region, but slots still within
    bad[0].slots[0].symbol.nid="BadNid0000";
    assert(!bindSCE47Graph(bad,*catalog,fresh));
    assert(fresh.write64(0x150000,777) && fresh.write64(0x151000,999));
    assert(!fresh.read8(0x200000));
    std::cout<<"PASS: compatibility 47 dependency graph, exact NIDs and rollback\n";
}
