#include "GuestGCNLanes.hpp"
#include <cstdlib>
#include <iostream>
namespace{int n=0;void yes(bool v,const char*s){++n;if(!v){std::cerr<<"FAIL38: "<<s<<'\n';std::exit(1);}}}
int main(){using namespace misaki;
 auto r=runGCNLaneMemoryDiagnostic();yes(bool(r),"four lane run");
 yes(r->activeMask==15 && r->executedLanes==4 && r->instructions==44,"lane count and instruction budget");
 yes(r->indices==std::array<std::uint32_t,4>{{3,7,11,15}},"independent lane addresses");
 for(unsigned i=0;i<4;++i){yes(r->loaded[i]==r->indices[i]*3+10,"read each lane");yes(r->stored[i]==r->loaded[i],"write each lane");yes(r->output[r->indices[i]]==r->stored[i],"output record matches");}
 yes(r->output[4]==0xB0000004u && r->output[6]==0xB0000006u,"other records intact");
 yes(r->codeProtected&&r->descriptorsProtected&&r->inputProtected,"immutable shader, descriptors, source");
 yes(r->checksum!=0,"nonzero checksum");
 yes(runGCNLaneMemoryDiagnostic()->checksum==r->checksum,"deterministic");
 auto partial=runGCNLaneMemoryDiagnostic(0b0101);
 yes(partial && partial->executedLanes==2 && partial->instructions==22,"masked lanes");
 yes(partial->indices[0]==3 && partial->indices[2]==11,"enabled lanes");
 yes(partial->indices[1]==0 && partial->indices[3]==0,"disabled lanes untouched");
 yes(partial->output[7]==0xB0000007u && partial->output[15]==0xB000000Fu,"mask blocks stores");
 yes(!runGCNLaneMemoryDiagnostic(0),"no active lanes rejected");
 yes(!runGCNLaneMemoryDiagnostic(16),"outside mask rejected");
 for(unsigned lane=0;lane<4;++lane){const auto a=runGCNLaneMemoryDiagnostic(1u<<lane);
 yes(a && a->executedLanes==1 && a->instructions==11,"single lane");
 yes(a->output[r->indices[lane]]==r->stored[lane],"single lane output");}
 std::cout<<"PASS: "<<n<<" bounded GCN lane checks\n";
}
