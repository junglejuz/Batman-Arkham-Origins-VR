// Origins PERCAPTURE (see reflect.h). Same method as City's reflect.cpp; Origins' own address and layout:
// UpdateRequired = City's code with the probe's TimeBetweenCaptures at +0x54, LastCaptureTime +0x50 and the
// family time at +0x24; CaptureScene (vtbl[1] 0x7FDCF0) stamps +0x50 from it. Origins adds a static-capture
// flag (+0xB0) that only blocks updates while the game's Reflections option is off.
#include "reflect.h"
#include "core/util/log.h"
#include <windows.h>
#include <MinHook.h>
#include <imgui.h>
#include <atomic>
#include <cstdio>
#include <cstring>

namespace bvr::bo::reflect {
namespace {
using UpdateFn=int(__fastcall*)(void* probe,void* edx,const void* family);
UpdateFn updateOriginal=nullptr;
constexpr uintptr_t kUpdateRequired=0x3fdb90;
constexpr BYTE kPrologue[16]={0x83,0xec,0x18,0x53,0x55,0x8b,0xd9,0x83,0x7b,0x3c,0x00,0x56,0x8b,0x74,0x24,0x28};
std::atomic<bool> on{true},hooked{false};
std::atomic<int> eye{0};
std::atomic<uint32_t> pass{0},leftCaptures{0},forced{0},rightSkips{0};
struct LeftUpdate { void* probe; uint32_t pass; };
LeftUpdate leftUpdates[16]{}; // render thread only
unsigned leftUpdateN=0;
bool loggedForced=false;
void save() {
    wchar_t path[MAX_PATH]{}; swprintf_s(path,L"%s\\settings.ini",log::data_dir());
    WritePrivateProfileStringW(L"view",L"reflect_per_eye",on.load() ? L"1" : L"0",path);
}

int __fastcall update_detour(void* probe,void* edx,const void* family) {
    const int r=updateOriginal(probe,edx,family);
    const int e=eye.load(std::memory_order_relaxed);
    const uint32_t p=pass.load(std::memory_order_relaxed);
    if(e<0) {
        if(r) { leftCaptures++; leftUpdates[leftUpdateN++%16]={probe,p}; }
        return r;
    }
    if(e>0 && !r && on.load(std::memory_order_relaxed)) {
        // Updated in this frame's left pass (the pass just before) -> render it again for the right eye.
        for(const LeftUpdate& u:leftUpdates) if(u.probe==probe && p-u.pass<=2) {
            forced++;
            if(!loggedForced) { loggedForced=true; BVR_LOG("[bo] reflections: first right-eye re-capture (probe %p)",probe); }
            return 1;
        }
        rightSkips++;
    }
    return r;
}
}

bool install(uintptr_t imageBase) {
    wchar_t path[MAX_PATH]{}; swprintf_s(path,L"%s\\settings.ini",log::data_dir());
    on.store(GetPrivateProfileIntW(L"view",L"reflect_per_eye",1,path)!=0);
    void* target=reinterpret_cast<void*>(imageBase+kUpdateRequired);
    BYTE actual[16]{};
    __try { memcpy(actual,target,16); } __except(EXCEPTION_EXECUTE_HANDLER) { return false; }
    if(memcmp(actual,kPrologue,16)) {
        BVR_LOG("[bo] reflections: update check at RVA %08X has unexpected bytes - per-eye capture OFF",unsigned(kUpdateRequired));
        return false;
    }
    MH_STATUS s=MH_CreateHook(target,reinterpret_cast<void*>(&update_detour),reinterpret_cast<void**>(&updateOriginal));
    if(s==MH_OK) s=MH_EnableHook(target);
    hooked.store(s==MH_OK);
    BVR_LOG("[bo] hook reflection capture update check RVA %08X: %s",unsigned(kUpdateRequired),MH_StatusToString(s));
    return s==MH_OK;
}
void set_render_pass(uint32_t p,int eyeSign) {
    pass.store(p,std::memory_order_relaxed);
    eye.store(eyeSign,std::memory_order_relaxed);
}
void status() {
    BVR_LOG("[bo] reflections per eye %d (hook %d): left-pass captures %u, right-pass re-captures %u, right skips %u",
        on.load(),hooked.load(),leftCaptures.load(),forced.load(),rightSkips.load());
}
bool command(const char* args) {
    if(!strcmp(args,"on")) { on.store(true); save(); }
    else if(!strcmp(args,"off")) { on.store(false); save(); }
    else if(strcmp(args,"status")) return false;
    status();
    return true;
}
void draw_ui() {
    bool v=on.load();
    if(ImGui::Checkbox("puddle reflections drawn for each eye (leave on)",&v)) { on.store(v); save(); status(); }
    if(!hooked.load()) ImGui::TextDisabled("   (not available in this game version)");
}
}
