// City opening-logo method, guarded against Origins' actual movie player.
// Never truncate videos: City's forced EOF hung its CRI decoder.
#include "startup.h"
#include "startup_policy.h"
#include "core/input/xinput_bridge.h"
#include "core/util/log.h"
#include <windows.h>
#include <Xinput.h>
#include <MinHook.h>
#include <atomic>
#include <cstring>
namespace bvr::bo::startup {
namespace {
uintptr_t imageBase=0;
std::atomic<bool> enabled{false},skipped{false};
std::atomic<bool> logoObserved{false};
std::atomic<HANDLE> logos[4]{};
std::atomic<unsigned> nextHandle{0};
SRWLOCK padLock=SRWLOCK_INIT;
ButtonPolicy policy;
using Open=HANDLE(WINAPI*)(LPCWSTR,DWORD,DWORD,LPSECURITY_ATTRIBUTES,DWORD,DWORD,HANDLE);
using Read=BOOL(WINAPI*)(HANDLE,LPVOID,DWORD,LPDWORD,LPOVERLAPPED);
using Close=BOOL(WINAPI*)(HANDLE);
using GetState=DWORD(WINAPI*)(DWORD,XINPUT_STATE*);
using Stop=void(__thiscall*)(void*,float,int,int);
Open originalOpen=nullptr; Read originalRead=nullptr; Close originalClose=nullptr;
GetState getState=nullptr;
bool stop_guard() {
    // Complete 49-byte Origins stop body; only the InterlockedExchange IAT
    // operand relocates. bWait=false bypasses every cleanup/virtual call.
    const unsigned char expected[]={0x56,0x8b,0xf1,0x6a,0x02,0x8d,0x46,0x3c,0x50,0xff,0x15,
        0x30,0x52,0x39,0x01,0x83,0x7c,0x24,0x0c,0x00,0x74,0x10,0x8b,0x4e,0x40,0x85,0xc9,
        0x75,0x09,0x8b,0x16,0x8b,0x42,0x18,0x8b,0xce,0xff,0xd0,0xc7,0x46,0x7c,0,0,0,0,0x5e,0xc2,0x0c,0};
    unsigned char relocated[sizeof expected]; memcpy(relocated,expected,sizeof expected);
    const uint32_t iat=uint32_t(imageBase+0xf95230); memcpy(relocated+11,&iat,4);
    __try { return !memcmp(reinterpret_cast<void*>(imageBase+0x7c64c0),relocated,sizeof relocated); }
    __except(EXCEPTION_EXECUTE_HANDLER) { return false; }
}
void* startup_player() {
    __try {
        auto* player=*reinterpret_cast<unsigned char**>(imageBase+0x29c6ee4);
        if(!player || *reinterpret_cast<uintptr_t*>(player)!=imageBase+0x11dd410) return nullptr;
        auto* vt=*reinterpret_cast<uintptr_t**>(player);
        if(vt[5]!=imageBase+0x7c64c0) return nullptr;
        // Origins GFx FString movie name, verified by IsMoviePlaying's comparisons.
        const auto* name=*reinterpret_cast<const wchar_t**>(player+0x14);
        const int length=*reinterpret_cast<int*>(player+0x18);
        if(!name || length<=0 || length>MAX_PATH || name[length-1]!=0) return nullptr;
        if(!startup_name(name) || *reinterpret_cast<int*>(player+0x40)!=0) return nullptr;
        return player;
    } __except(EXCEPTION_EXECUTE_HANDLER) { return nullptr; }
}
bool logo_handle(HANDLE h) {
    if(!h || h==INVALID_HANDLE_VALUE) return false;
    for(auto& slot:logos) if(slot.load()==h) return true;
    return false;
}
bool logo_active() {
    // Decoder may close the file after buffering, before the video finishes.
    // The current engine movie name/finished state owns lifetime, not file I/O.
    return logoObserved.load() && startup_player()!=nullptr;
}
bool stop_player(void* player) {
    __try {
        // STATIC: 0 delay, no wait => atomic state 2 and queue flag reset only.
        reinterpret_cast<Stop>(imageBase+0x7c64c0)(player,0.0f,0,1);
        return true;
    } __except(EXCEPTION_EXECUTE_HANDLER) { return false; }
}
void filter(unsigned index,uint16_t* buttons) {
    if(!enabled.load() || !buttons || index>=4) return;
    AcquireSRWLockExclusive(&padLock);
    const bool request=policy.sample(index,*buttons,logo_active());
    ReleaseSRWLockExclusive(&padLock);
    if(!request) return;
    void* player=startup_player();
    if(player && stop_player(player)) {
        skipped.store(true);
        BVR_LOG("[bo] opening titles skipped by gamepad %u (Origins non-waiting movie stop, thread %lu)",index,GetCurrentThreadId());
    } else {
        enabled.store(false);
        BVR_LOG("[bo] opening-title skip disabled: movie player changed before stop");
    }
}
HANDLE WINAPI open(LPCWSTR path,DWORD access,DWORD share,LPSECURITY_ATTRIBUTES sa,DWORD creation,DWORD flags,HANDLE t) {
    if(skipped.load() && startup_name(path)) { SetLastError(ERROR_FILE_NOT_FOUND); return INVALID_HANDLE_VALUE; }
    HANDLE h=originalOpen(path,access,share,sa,creation,flags,t);
    const DWORD error=GetLastError();
    if(h!=INVALID_HANDLE_VALUE && logo_name(path)) {
        logoObserved.store(true);
        logos[nextHandle.fetch_add(1)%4].store(h);
        BVR_LOG("[bo] opening-logo video opened: %ls",path);
    }
    SetLastError(error);
    return h;
}
BOOL WINAPI read(HANDLE h,LPVOID bytes,DWORD count,LPDWORD got,LPOVERLAPPED ov) {
    const DWORD saved=GetLastError();
    if(logo_handle(h)) tick();
    SetLastError(saved);
    return originalRead(h,bytes,count,got,ov); // decoder I/O always intact
}
BOOL WINAPI close(HANDLE h) {
    const BOOL result=originalClose(h); const DWORD error=GetLastError();
    if(result) for(auto& slot:logos) { HANDLE expected=h; slot.compare_exchange_strong(expected,nullptr); }
    SetLastError(error); return result;
}
}
void tick() {
    if(!enabled.load() || !getState) return;
    static std::atomic<ULONGLONG> last{0};
    const ULONGLONG now=GetTickCount64(); ULONGLONG before=last.load();
    if(now-before<16 || !last.compare_exchange_strong(before,now)) return;
    // Also polls during logo buffering / a blocking startup game-thread wait.
    input::set_menu_poll(true);
    for(DWORD i=0;i<4;++i) {
        XINPUT_STATE state{};
        if(getState(i,&state)==ERROR_SUCCESS) filter(i,&state.Gamepad.wButtons);
    }
    input::set_menu_poll(false);
}
void install() {
    imageBase=reinterpret_cast<uintptr_t>(GetModuleHandleW(nullptr));
    if(!stop_guard()) { BVR_LOG("[bo] startup movie stop guard failed; stock logos retained"); return; }
    wchar_t system[MAX_PATH]{}; GetSystemDirectoryW(system,MAX_PATH); wcscat_s(system,L"\\xinput1_4.dll");
    if(auto m=LoadLibraryW(system)) getState=reinterpret_cast<GetState>(GetProcAddress(m,"XInputGetState"));
    struct Hook { const char* name; void* detour; void** original; void* target; } hooks[]={
        {"CreateFileW",reinterpret_cast<void*>(open),reinterpret_cast<void**>(&originalOpen)},
        {"ReadFile",reinterpret_cast<void*>(read),reinterpret_cast<void**>(&originalRead)},
        {"CloseHandle",reinterpret_cast<void*>(close),reinterpret_cast<void**>(&originalClose)}};
    bool ok=true;
    for(auto& h:hooks) {
        h.target=reinterpret_cast<void*>(GetProcAddress(GetModuleHandleW(L"kernel32.dll"),h.name));
        if(MH_CreateHook(h.target,h.detour,h.original)!=MH_OK) { ok=false; break; }
    }
    if(ok) for(auto& h:hooks) if(MH_QueueEnableHook(h.target)!=MH_OK) ok=false;
    if(ok) ok=MH_ApplyQueued()==MH_OK;
    if(!ok) {
        for(auto& h:hooks) if(h.target) { MH_DisableHook(h.target); MH_RemoveHook(h.target); }
        BVR_LOG("[bo] opening-title hooks failed; stock movie I/O retained"); return;
    }
    input::set_button_filter(filter); enabled.store(true);
    BVR_LOG("[bo] opening titles: A/B/X/Y, Start or Back skips; only Startup movie + known logo files qualify");
}
}
