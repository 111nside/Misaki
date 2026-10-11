#include "GuestSCE52.hpp"
#include "GuestSCE46.hpp"
#include <iostream>
#include <cstdlib>
using namespace misaki;
static int checks=0;
static void check(bool ok,const char *why){++checks;if(!ok){std::cerr<<"FAIL M52: "<<why<<'\n';std::exit(1);}}
static void put(std::vector<std::uint8_t> &bytes,std::size_t at,std::uint64_t v,unsigned count){
    for(unsigned k=0;k<count;++k)bytes.at(at+k)=static_cast<std::uint8_t>(v>>(8*k));
}
int main(){
    auto fixture=makeSCE46Fixture();
    auto tags=inspectSCE52Dynamic(fixture.data(),fixture.size());
    check(tags && tags->entries==3 && tags->ended,"DT_RELA tags inspected");
    check(tags->sceTags==0 && tags->neededModules==0,"ordinary ELF tags distinguished");
    auto version=fixture;
    put(version,0x330,0x6100000d,8); put(version,0x338,0x1234,8);
    put(version,0x340,0x6100000f,8); put(version,0x348,0x5678,8);
    put(version,0x350,0x61000015,8); put(version,0x358,0xABCD,8);
    auto sce=inspectSCE52Dynamic(version.data(),version.size());
    check(sce && sce->sceTags==3 && sce->neededModules==1 &&
          sce->importLibraries==1 && sce->moduleInfo==0x1234 &&
          sce->firstNeeded==0x5678,"SCE tag names recognized");
    auto duplicate=version;
    put(duplicate,0x340,0x6100000d,8);
    check(!inspectSCE52Dynamic(duplicate.data(),duplicate.size()),"duplicate module info rejected");
    auto noEnd=version;
    put(noEnd,0x360,0x6100000f,8);
    check(!inspectSCE52Dynamic(noEnd.data(),noEnd.size()),"unterminated dynamic rejected");
    auto invalidSize=fixture;
    put(invalidSize,64+2*56+32,15,8);
    check(!inspectSCE52Dynamic(invalidSize.data(),invalidSize.size()),"misaligned table rejected");
    auto key=makePS4SyntheticKey(0);
    auto symbol=parseSCE52Symbol(key.nid+"#"+key.library+"#"+key.module);
    check(symbol && symbol->encodedNid==key.nid && symbol->module==key.module,"valid NID structure");
    check(!parseSCE52Symbol("short#libSceA#libSceB"),"wrong NID length");
    check(!parseSCE52Symbol(key.nid+"#bad space#module"),"unsafe characters");
    check(!parseSCE52Symbol(key.nid+"##module"),"missing library");
    check(!parseSCE52Symbol(key.nid+"#lib#module#extra"),"extra delimiter");
    GuestMemory memory;
    check(installPS4SyntheticImportFunctions(memory),"synthetic export code mapped");
    auto catalog=makePS4SyntheticImportCatalog(memory);
    check(bool(catalog),"fixture catalog built");
    std::vector<PS4ImportKey> keys{makePS4SyntheticKey(0),makePS4SyntheticKey(1)};
    auto result=resolveSCE52(keys,*catalog);
    check(result && result->matched==2 && result->firstAddress==0x9000,"two exact bindings");
    check(!resolveSCE52({},*catalog),"empty requests refused");
    keys[1].minor=99;
    check(!resolveSCE52(keys,*catalog),"wrong version not resolved");
    keys[1]=keys[0];
    check(!resolveSCE52(keys,*catalog),"duplicate keys refused");
    keys[1]=makePS4SyntheticKey(1);
    keys[1].nid="BadNid";
    check(!resolveSCE52(keys,*catalog),"bad NID size refused");
    std::cout<<"PASS: "<<checks<<" SCE52 checks\n";
}
