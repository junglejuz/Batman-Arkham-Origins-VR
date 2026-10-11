#pragma once
#include <cstdint>
#include <cwchar>
namespace bvr::bo::startup {
constexpr uint16_t skipButtons=0xf030; // A/B/X/Y, Start, Back; no gameplay sticks/triggers
inline const wchar_t* basename(const wchar_t* path) {
    const wchar_t* name=path;
    if(path) for(auto* p=path;*p;++p) if(*p==L'\\' || *p==L'/') name=p+1;
    return name;
}
inline bool startup_name(const wchar_t* path) {
    const auto* n=basename(path);
    return n && (!_wcsicmp(n,L"Startup") || !_wcsicmp(n,L"Startup.swf"));
}
inline bool logo_name(const wchar_t* path) {
    const auto* n=basename(path);
    return n && (!_wcsicmp(n,L"bac_logo.usm") || !_wcsicmp(n,L"bac_logo_PC.usm") ||
                 !_wcsicmp(n,L"bac_logo_fra_PC.usm"));
}
struct ButtonPolicy {
    struct Pad { uint16_t last=0,owned=0; bool seen=false; } pads[4];
    bool skipped=false;
    // The press owner keeps consuming held buttons after the movie/menu changes.
    bool sample(unsigned index,uint16_t& buttons,bool startupActive) {
        if(index>=4) return false;
        auto& p=pads[index]; const uint16_t raw=buttons & skipButtons;
        const uint16_t rising=p.seen ? raw & ~p.last : 0;
        p.seen=true; p.last=raw; p.owned &= raw;
        const bool skip=startupActive && !skipped && rising;
        if(skip) { skipped=true; p.owned |= raw; }
        buttons &= ~p.owned;
        return skip;
    }
};
}
