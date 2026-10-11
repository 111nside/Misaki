#include "GuestPS4Formats.hpp"
#include <cstdlib>
#include <iostream>
#include <vector>
#include <limits>
namespace { int n=0; void yes(bool v,const char*s){++n;if(!v){std::cerr<<"FAIL36: "<<s<<'\n';std::exit(1);}} }
int main(){using namespace misaki;
 auto f=makePS4StructuralFixture();std::string e;
 auto r=inspectPS4Executable(f.data(),f.size(),&e);
 yes(r.has_value(),"SELF structure");yes(r->kind==PS4ContainerKind::self,"container kind");
 yes(r->elfType==0xFE10 && r->machine==62,"SCE dynexec AMD64");
 yes(r->selfSegments==1 && r->programHeaders==1,"segment counts");
 yes(r->elfHeaderOffset==64 && r->entry==0x400000,"embedded ELF offset/entry");
 yes(!r->readyToExecute && r->structuralOnly,"never mark runnable");
 auto enc=makePS4StructuralFixture(true);auto rr=inspectPS4Executable(enc.data(),enc.size());
 yes(rr && rr->encryptedSegments==1 && !rr->readyToExecute,"encrypted metadata recognized, not decrypted");
 yes(!inspectPS4Executable(nullptr,0),"null rejected");
 yes(!inspectPS4Executable(f.data(),25),"short header rejected");
 f[4]=5;yes(!inspectPS4Executable(f.data(),f.size()),"unknown version"); f[4]=0;
 f[24]=200;yes(!inspectPS4Executable(f.data(),f.size()),"many segments"); f[24]=1;
 f[32]=0x20;yes(!inspectPS4Executable(f.data(),f.size()),"unsupported flag"); f[32]=0;
 f[40]=0xff;f[41]=0xff;yes(!inspectPS4Executable(f.data(),f.size()),"segment out of bounds");f[40]=0;f[41]=2;
 f[64+4]=1;yes(!inspectPS4Executable(f.data(),f.size()),"ELF class");f[64+4]=2;
 f[64+16]=3;f[64+17]=0;yes(!inspectPS4Executable(f.data(),f.size()),"ordinary ELF rejected");
 f[64+16]=0x10;f[64+17]=0xFE;
 f[64+54]=5;yes(!inspectPS4Executable(f.data(),f.size()),"program header entry size");
 f=makePS4StructuralFixture();yes(inspectPS4Executable(f.data()+64,f.size()-64)->kind==PS4ContainerKind::sceELF,"standalone SCE ELF");
 std::cout<<"PASS: "<<n<<" SELF structural checks\n";
}
