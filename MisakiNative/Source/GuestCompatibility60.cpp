#include "../Include/GuestCompatibility60.hpp"
#include "../Include/GuestSCE51.hpp"
#include "../Include/GuestSCE56.hpp"
#include <algorithm>
#include <limits>
#include <sstream>

namespace misaki { namespace {
std::uint64_t rd(const std::uint8_t *p,std::size_t off,unsigned bytes){
    std::uint64_t result=0;for(unsigned i=0;i<bytes;++i)result|=std::uint64_t(p[off+i])<<(8*i);return result;
}
bool span(std::uint64_t at,std::uint64_t n,std::uint64_t len){return at<=len&&n<=len-at;}
void add(Compat60Result&r,std::uint32_t code,const std::string&message){
    if(r.findings.size()<16)r.findings.push_back({code,message});
}
const char *kindName(Compat60Kind kind){
    switch(kind){case Compat60Kind::sceELF:return "SCE ELF64";case Compat60Kind::self:return "PS4 SELF";case Compat60Kind::ordinaryELF:return "ELF64 (not PS4)";default:return "Unrecognized";}
}
}
Compat60Result analyzeExecutable60(const std::uint8_t *b,std::size_t count,std::uint64_t actual){
    Compat60Result r;r.inspectedBytes=count;r.declaredBytes=actual;
    if(!b||count<4||count>8*1024*1024||actual<count){
        add(r,1,"Missing, oversized, or inconsistent inspection input");
    }else{
        r.partial=actual>count;
        std::uint64_t elfOff=0;
        bool hasELF=false;
        if(rd(b,0,4)==0x1D3D154Fu){
            r.kind=Compat60Kind::self;r.formatRecognized=true;
            if(count<32){add(r,9,"SELF header is not fully in the inspected prefix");}
            else{
                const auto records=rd(b,24,2);
                if(records==0||records>128 || !span(32,records*32,count)){
                    add(r,1,"Invalid or unavailable SELF segment-table metadata");
                }else{
                    for(std::uint64_t i=0;i<records;++i){
                        const auto offset=static_cast<std::size_t>(32+i*32);
                        const auto flags=rd(b,offset,8);
                        if(flags&2)++r.encryptedSegments;
                        if(flags&8)++r.compressedSegments;
                        const auto file=rd(b,offset+8,8),length=rd(b,offset+16,8);
                        if(!span(file,length,actual))add(r,1,"SELF segment exceeds declared file length");
                    }
                    elfOff=32+records*32;
                    hasELF=span(elfOff,64,count);
                    if(!hasELF)add(r,9,"Embedded ELF header is outside the inspected prefix");
                }
            }
            if(r.encryptedSegments){r.encryptedRefused=true;add(r,2,"Encrypted SELF payload: metadata only; decryption not implemented");}
            if(r.compressedSegments)add(r,3,"Compressed SELF payload: decompression not implemented");
        }else if(count>=4&&b[0]==0x7f&&b[1]=='E'&&b[2]=='L'&&b[3]=='F'){
            hasELF=true;
        }else add(r,1,"Not a recognized SELF or ELF magic header");
        if(hasELF){
            if(!span(elfOff,64,count) || b[elfOff+4]!=2||b[elfOff+5]!=1||b[elfOff+6]!=1){
                add(r,4,"Only ELF64 little-endian metadata is supported");
            }else{
                r.elfType=static_cast<std::uint16_t>(rd(b,elfOff+16,2));
                const auto machine=rd(b,elfOff+18,2),phoff=rd(b,elfOff+32,8);
                const auto n=rd(b,elfOff+56,2),entrySize=rd(b,elfOff+54,2);
                const bool sce=r.elfType==0xfe00||r.elfType==0xfe10||r.elfType==0xfe18;
                if(r.kind!=Compat60Kind::self){
                    r.kind=sce?Compat60Kind::sceELF:Compat60Kind::ordinaryELF;
                    r.formatRecognized=true;
                }
                if(machine!=62||!sce)add(r,4,"Executable is not a recognized x86-64 SCE ELF type");
                if(n>128||entrySize!=56||phoff>UINT64_MAX-elfOff||
                   !span(elfOff+phoff,n*56,actual)){
                    add(r,4,"Invalid ELF program header layout");
                }else if(!span(elfOff+phoff,n*56,count)){
                    add(r,9,"ELF program headers exceed the inspected prefix");
                }else{
                    for(std::uint64_t i=0;i<n;++i){
                        const auto at=static_cast<std::size_t>(elfOff+phoff+i*56);
                        const auto type=rd(b,at,4);
                        if(type==1)++r.loadSegments;
                        if(type==2)++r.dynamicSegments;
                    }
                    if(r.kind==Compat60Kind::sceELF && !r.partial){
                        std::string reason;
                        const auto dyn=inspectSCE56Dynamic(b,count,&reason);
                        if(dyn){
                            r.neededModules=dyn->neededNames;
                            if(dyn->sceModuleCount)add(r,5,"SCE dependency identifiers need external module metadata and real exports");
                            if(dyn->hasUnsupportedRelocations)add(r,10,"Unsupported dynamic relocation or tag semantics detected");
                        }else if(r.dynamicSegments){
                            add(r,10,"Detailed dynamic metadata not loadable: "+reason);
                        }
                    }
                }
            }
        }
        if(r.partial)add(r,9,"Inspected a bounded file prefix, not the entire executable");
    }
    if(r.kind==Compat60Kind::sceELF||r.kind==Compat60Kind::self){
        if(r.neededModules.empty())add(r,5,"Required import modules cannot yet be enumerated or resolved fully");
        else add(r,5,"Imported modules are listed, but authentic Sony exports are unavailable");
        add(r,6,"Authentic PS4 kernel, threads, and system libraries are not implemented");
        add(r,7,"Full AMD GCN shader, wavefront, and graphics pipeline not implemented");
        add(r,10,"Sony SELF execution, SCE-specific linking, and startup are incomplete");
    }
    add(r,8,"Actual PS4 firmware boot and the original home menu are unsupported");
    std::ostringstream out;
    out<<"Detected: "<<kindName(r.kind)<<"\n"
       <<"ELF type: 0x"<<std::hex<<r.elfType<<std::dec<<"\n"
       <<"Bytes inspected: "<<r.inspectedBytes<<" / "<<r.declaredBytes<<"\n"
       <<"Load / dynamic segments: "<<r.loadSegments<<" / "<<r.dynamicSegments<<"\n"
       <<"Encrypted / compressed SELF segments: "<<r.encryptedSegments<<" / "<<r.compressedSegments<<"\n"
       <<"Dynamic dependencies: "<<r.neededModules.size()<<"\n";
    for(const auto &name:r.neededModules)out<<"  - "<<name<<" (unresolved)\n";
    out<<"\nCompatibility blockers:\n";
    for(const auto &finding:r.findings)out<<"  - "<<finding.description<<"\n";
    out<<"\nNo guest executable or firmware was executed.\n";
    r.summary=out.str();
    r.canExecute=false;
    return r;
}
} // namespace misaki
