#include "wipe.h"
#include <cstring>
#include <algorithm>
#include <utility>
namespace bvr::bo::wipe {
bool Recording::decode_frame(unsigned i,std::vector<uint8_t>& pixels) const {
    if(i>=times.size()) return false;
    const size_t size=size_t(width)*height;
    pixels.resize(size);size_t out=0;
    for(size_t at=offsets[i];at<offsets[i]+lengths[i];at+=2) {
        const unsigned n=bytes[at];
        if(!n || n>size-out) return false;
        memset(pixels.data()+out,bytes[at+1],n);out+=n;
    }
    return out==size;
}
bool decode(std::vector<uint8_t> bytes,Recording& result) {
    result={};size_t at=0;
    auto word=[&](uint32_t& value) {
        if(at>bytes.size() || bytes.size()-at<4) return false;
        memcpy(&value,bytes.data()+at,4);at+=4;return true;
    };
    uint32_t magic=0,version=0,w=0,h=0,n=0;
    if(!word(magic)||!word(version)||!word(w)||!word(h)||!word(n)||magic!=0x50574342 ||version!=2 ||
       !w||!h||w>4096||h>4096||n<8||n>160) return false;
    Recording r;r.width=w;r.height=h;r.times.resize(n);
    for(unsigned i=0;i<n;++i) if(!word(r.times[i]) || (i && r.times[i]<=r.times[i-1])) return false;
    if(r.times[0]!=0 || r.times.back()>5000 || r.times.back()<100) return false;
    for(unsigned i=0;i<n;++i) {
        uint32_t len=0;
        if(!word(len)||!len ||len%2 ||len>size_t(w)*h*2 ||len>bytes.size()-at) return false;
        size_t decoded=0;
        for(size_t k=at;k<at+len;k+=2) {
            if(!bytes[k] || bytes[k]>size_t(w)*h-decoded) return false;
            decoded+=bytes[k];
        }
        if(decoded!=size_t(w)*h) return false;
        r.offsets.push_back(at);r.lengths.push_back(len);at+=len;
    }
    if(at!=bytes.size()) return false;
    r.bytes=std::move(bytes);result=std::move(r);return true;
}
void Playback::event(uint64_t now) {
    phase=first ? Phase::Reveal : Phase::Cover;
    blackPending=first;
    beginPending=true;
    first=false;start=now;holdStart=projectionSince=0;
}
bool Playback::owns() const {
    return phase!=Phase::Idle && phase!=Phase::AwaitMenu;
}
Step Playback::tick(uint64_t now,bool projection,unsigned duration) {
    if(phase==Phase::AwaitMenu) return {};
    if(phase==Phase::Idle) return {};
    if(now<start) return {Visual::Black};
    // Sound can precede a blocked/loading render. Start at presentation so a
    // late first frame cannot consume the entire bat animation unseen.
    if(beginPending) {beginPending=false;start=now;}
    if(!projection) projectionSince=0;
    else if(!projectionSince) projectionSince=now;
    if(phase==Phase::Reveal && !projection) {
        phase=Phase::Hold;holdStart=now;return {Visual::Black};
    }
    if(phase==Phase::Cover && now-start>=duration) {
        phase=Phase::Hold;holdStart=now;return {Visual::Black};
    }
    if(phase==Phase::Hold) {
        // Actual projection verdict owns return from a loading/cinematic
        // screen. A bounded settle also supports a menu which stays 3D.
        // BATSFLOW (JJ 2026-10-26, end of the fast-travel video: bats "then faded to black and then bats played again ... not
        // very clean"): the 350 ms settle counts from when the picture came back, not from the end of the cover, so a picture
        // that is already steady when the bats finish covering is revealed at once - one bats-in / bats-out, no black pause.
        if(projection && now-start>=duration && now-projectionSince>=350) {
            phase=Phase::Reveal;start=now;blackPending=false;return {Visual::Black};
        }
        if(now-holdStart>=60000) {phase=Phase::Idle;return {};}
        return {Visual::Black};
    }
    if(blackPending) {blackPending=false;start=now;return {Visual::Black};}
    const uint64_t elapsed=now-start;
    if(phase==Phase::Reveal && elapsed>=duration) {phase=Phase::Idle;return {};}
    if(elapsed<20) return {phase==Phase::Reveal ? Visual::Black : Visual::Animation,phase==Phase::Reveal,elapsed};
    return {Visual::Animation,phase==Phase::Reveal,elapsed};
}
}
