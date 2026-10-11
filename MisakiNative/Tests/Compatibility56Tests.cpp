#include "GuestSCE56.hpp"
#include "GuestSCE46.hpp"
#include "GuestPS4Formats.hpp"
#include <iostream>
#include <cstdlib>
using namespace misaki;
static unsigned checks=0;
static void ok(bool x,const char *s){++checks;if(!x){std::cerr<<"FAIL 56: "<<s<<'\n';std::exit(1);}}
int main(){
 auto b=makeSCE56Fixture();auto r=inspectSCE56Dynamic(b.data(),b.size());
 ok(bool(r),"dynamic recognized");
 ok(r->neededNames.size()==1&&r->neededNames[0]=="libDemo.prx","ELF DT_NEEDED name");
 ok(r->relativeTableEntries==1&&r->relocationTableConsistent,"RELA metadata");
 ok(r->loadSegments==2&&r->hasStringTable,"mapped dynamic table");
 ok(!r->hasUnsupportedRelocations,"fixture only uses supported tags");
 auto invalid=b; invalid[0x3D8]=0xff;
 ok(!inspectSCE56Dynamic(invalid.data(),invalid.size()),"reject out of table string offset");
 invalid=b;invalid[0x398]=0x1b;
 ok(!inspectSCE56Dynamic(invalid.data(),invalid.size()),"reject unsafe dependency name");
 invalid=b;invalid[0x3F8]=25;
 ok(!inspectSCE56Dynamic(invalid.data(),invalid.size()),"reject misaligned relocation size");
 invalid=b; invalid[0x400]=0;
 ok(!inspectSCE56Dynamic(invalid.data(),invalid.size()),"reject missing relaent");
 invalid=b; invalid.resize(0x3ff);
 ok(!inspectSCE56Dynamic(invalid.data(),invalid.size()),"reject truncated segments");
 auto self=makePS4StructuralFixture(true);
 ok(!inspectSCE56Dynamic(self.data(),self.size()),"SELF cannot be used as ELF dynamic table");
 auto old=makeSCE46Fixture();GuestMemory mem;
 ok(bool(loadSCE46(old.data(),old.size(),0x100000,mem)),"prior executable loader retained");
 std::cout<<"PASS "<<checks<<" M56 checks\n";
}
