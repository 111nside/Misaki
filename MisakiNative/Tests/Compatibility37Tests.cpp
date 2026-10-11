#include "GuestPS4Imports.hpp"
#include <cstdlib>
#include <iostream>
namespace {int n=0;void yes(bool v,const char *s){++n;if(!v){std::cerr<<"FAIL37: "<<s<<'\n';std::exit(1);}}}
int main(){ using namespace misaki; GuestMemory mem;
 yes(installPS4SyntheticImportFunctions(mem),"map toy guest function images");
 auto catalog=makePS4SyntheticImportCatalog(mem);yes(bool(catalog),"construct synthetic import catalog");
 yes(catalog->size()==2,"two exports");
 auto a=makePS4SyntheticKey(0),b=makePS4SyntheticKey(1);
 yes(catalog->resolve(a)==0x9000,"first NID resolved");
 yes(catalog->resolve(b)==0x9010,"second NID resolved");
 auto batch=catalog->resolveAll({a,b});
 yes(batch && batch->size()==2 && (*batch)[0]==0x9000 && (*batch)[1]==0x9010,"atomic multi-import resolution");
 yes(!catalog->resolveAll({}),"empty batch rejected");
 b.major=2;yes(!catalog->resolve(b),"version mismatch unresolved");
 yes(!catalog->resolveAll({a,b}),"batch failure no partial result");b.major=1;
 b.nid="bad";yes(!catalog->resolve(b),"bad NID unresolved");
 PS4ImportCatalog temp;
 yes(!temp.define(mem,{b,0x9010}),"reject malformed encoded NID");b=makePS4SyntheticKey(1);
 yes(!catalog->define(mem,{a,0x9000}),"duplicate import rejected");
 yes(!catalog->define(mem,{b,0xA000}),"unmapped function pointer rejected");
 auto invalid=b;invalid.library="libUnknown";
 yes(!temp.define(mem,{invalid,0x9000}),"SCE-style naming validated");
 invalid=b;invalid.nid="???????????";
 yes(!temp.define(mem,{invalid,0x9000}),"NID token alphabet validated");
 invalid=b;invalid.module="libSceTestModule";invalid.major=0;
 yes(!temp.define(mem,{invalid,0x9000}),"version required");
 yes(!mem.writeBytes(0x9000,reinterpret_cast<const std::uint8_t*>("x"),1),"guest imports read-only");
 std::cout<<"PASS: "<<n<<" synthetic SCE-import checks\n";
}
