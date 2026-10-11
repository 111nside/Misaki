#include "GuestServices57.hpp"
#include "GuestSCE52.hpp"
#include <iostream>
#include <cstdlib>
using namespace misaki;
static unsigned checks=0;
static void ok(bool x,const char*s){++checks;if(!x){std::cerr<<"FAIL 57: "<<s<<'\n';std::exit(1);}}
int main(){
 ServiceRegistry57 r;
 auto a=makePS4SyntheticKey(0),b=makePS4SyntheticKey(1);
 ok(r.add({a,PrivateService57::pid}),"register valid test identity");
 ok(r.add({b,PrivateService57::write}),"register second identity");
 ok(!r.add({a,PrivateService57::exit}),"reject overwrite");
 ok(r.resolve(a)==PrivateService57::pid&&r.size()==2,"exact lookup");
 auto wrong=a;wrong.major+=1;
 ok(!r.resolve(wrong),"reject incorrect module version");
 wrong=a;wrong.minor+=1;
 ok(!r.resolve(wrong),"reject incorrect library minor");
 wrong=a;wrong.library+="x";
 ok(!r.resolve(wrong),"reject unknown library");
 wrong=a;wrong.nid="!!!";
 ok(!r.add({wrong,PrivateService57::pid}),"reject unsafe/invalid NID");
 auto result=runService57Diagnostic();
 ok(bool(result),"guest service dispatch works");
 ok(result->resolved==2&&result->calls==3&&result->failures==1,"service call isolation");
 ok(result->outputBytes==5&&result->processIsolated,"guest read-only buffer and process permission");
 std::cout<<"PASS "<<checks<<" M57 checks\n";
}
