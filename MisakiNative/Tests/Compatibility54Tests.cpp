#include "GuestGCN54.hpp"
#include <iostream>
#include <cstdlib>
using namespace misaki;
static int checks=0;
static void check(bool ok,const char *why){++checks;if(!ok){std::cerr<<"FAIL M54: "<<why<<'\n';std::exit(1);}}
int main(){
    check(!runGCN54(0),"zero lane mask refused");
    check(!runGCN54(16),"unknown GCN lanes refused");
    check(!runGCN54(0xFFFFFFFFu),"out-of-range lane mask refused");
    const auto full=runGCN54(15);
    check(bool(full),"four-lane PM4 and GCN pipeline succeeded");
    check(full->pm4Packets==11 && full->draws==1,"nested PM4 decoded");
    check(full->colorTarget==0x20000000 && full->shaderAddress==0x100000,"PM4 shader/target metadata used");
    check(full->lanes==4 && full->laneMask==15,"four masked software lanes");
    check(full->instructions==44,"eleven instructions per lane");
    check(full->indices==std::array<uint32_t,4>{{3,7,11,15}},"per-lane index registers");
    check(full->loaded==std::array<uint32_t,4>{{19,31,43,55}},"per-lane memory reads");
    check(full->output[3]==19 && full->output[7]==31 &&
          full->output[11]==43 && full->output[15]==55,"per-lane stores");
    check(full->output[4]==0xB0000004u && full->output[63]==0xB000003Fu,"untouched pixels preserved");
    check(full->shaderReadOnly && full->inputReadOnly && full->descriptorsReadOnly,"read-only resources");
    check(full->outputHash!=0,"output hash");
    for(unsigned lane=0;lane<4;++lane){
        const auto single=runGCN54(1u<<lane);
        check(single && single->lanes==1,"single masked lane executes");
        check(single->output[3+4*lane]==19+12*lane,"single lane expected output");
        check(single->instructions==11,"single lane instruction budget");
        check(single->output[7+(lane==1?1:0)]==
             (lane==1?0xB0000008u:0xB0000007u),"masked-off memory stays unchanged");
    }
    auto repeated=runGCN54(15);
    check(repeated && full->outputHash==repeated->outputHash,"deterministic checksum");
    std::cout<<"PASS: "<<checks<<" GCN54 checks\n";
}
