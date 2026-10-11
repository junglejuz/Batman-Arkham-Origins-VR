#include "game/origins/startup_policy.h"
#include <cstdio>
using namespace bvr::bo::startup;
int main() {
    if(!startup_name(L"..\\Movies\\Startup.swf") || !startup_name(L"startup") ||
       startup_name(L"Startup_MP.swf") || startup_name(L"ep2_0030.usm") ||
       !logo_name(L"..\\Movies\\bac_logo_PC.usm") || logo_name(L"bac_logo_mp.usm") ||
       logo_name(L"ep2_0030.usm")) return 1;
    ButtonPolicy policy; uint16_t a=0x1000;
    if(policy.sample(0,a,true) || a!=0x1000) return 2; // preheld at first sample
    a=0; if(policy.sample(0,a,true)) return 3;
    a=0x1001; if(!policy.sample(0,a,true) || a!=1) return 4;
    a=0x1001; if(policy.sample(0,a,false) || a!=1) return 5; // menu transition while held
    a=0; policy.sample(0,a,false);
    a=0x1000; if(policy.sample(0,a,false) || a!=0x1000) return 6; // fresh menu confirm works
    for(unsigned i=0;i<16;i++) {
        const uint16_t bit=uint16_t(1u<<i); ButtonPolicy p; uint16_t b=0;
        p.sample(2,b,false); b=bit;
        const bool result=p.sample(2,b,true);
        if(result!=bool(bit & skipButtons) || b!=(bit & skipButtons ? 0 : bit)) return 7;
    }
    ButtonPolicy story; a=0; story.sample(1,a,false); a=0x1000;
    if(story.sample(1,a,false) || a!=0x1000) return 8;
    if(story.sample(4,a,true) || a!=0x1000) return 9;
    puts("PASS: title-only names, button mask/edges, preheld input, consumed release across menu transition, fresh confirm, story/invalid-slot passthrough.");
}
