// Origins adapter. No City/Knight engine offsets or shader identities are used.
// STATIC contracts: retail SHA in RE_NOTES; constructor ret 0x70, matrices words 14/15.
#include "scene.h"
#include "afw.h"
#include "dlss_bridge.h"
#include "menu_state.h"
#include "reflect.h"
#include "core/vr/eye_burst.h"
#include "core/framework/command.h"
#include "core/gfx/gfx_hud.h"
#include "core/gfx/hud_capture.h"
#include "startup.h"
#include "transitions.h"
#include "core/hooks/d3d11_hook.h"
#include "core/hooks/pattern_scan.h"
#include "core/input/xinput_bridge.h"
#include "core/ui/overlay.h"
#include "core/util/log.h"
#include "core/vr/openxr_runtime.h"
#include <windows.h>
#include <d3d11.h>
#include <dxgi.h>
#include <MinHook.h>
#include <imgui.h>
#include <intrin.h>
#include <atomic>
#include <array>
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>

namespace bvr::bo::scene {
namespace {
using W=uint32_t;
using DrawFn=void(__fastcall*)(void*,void*,W);
using CalcFn=void*(__fastcall*)(void*,void*,void*,float*,int32_t*,void*,void*);
using BeginFn=W(__fastcall*)(void*,void*);
#define A28 W a1,W a2,W a3,W a4,W a5,W a6,W a7,W a8,W a9,W a10,W a11,W a12,W a13,W a14,W a15,W a16,W a17,W a18,W a19,W a20,W a21,W a22,W a23,W a24,W a25,W a26,W a27,W a28
#define P28(v,p) a1,a2,a3,a4,a5,a6,a7,a8,a9,a10,a11,a12,a13,v,p,a16,a17,a18,a19,a20,a21,a22,a23,a24,a25,a26,a27,a28
using CtorFn=void*(__fastcall*)(void*,void*,A28);
using AllocFn=void*(__cdecl*)();
DrawFn drawOriginal=nullptr;
CalcFn calcOriginal=nullptr;
CtorFn ctorOriginal=nullptr;
BeginFn beginOriginal=nullptr;
uintptr_t imageBase=0;
std::atomic<bool> installed{false},camera{true},native{true},poisoned{false},hudOn{true};
std::atomic<bool> mapRec{false}; // MAPREC: record every map opening / closing (test only since RELEASE 0.1.0: `bo maprec on`)
std::atomic<bool> viewDebounce{true}; // VIEWDEBOUNCE: one frame without a 3D view does not leave head-tracked mode
std::atomic<W> gameplayCaller{0},lastViewCaller{0},draws{0},seconds{0},edits{0},invalid{0},begins{0},tagMiss{0};
std::atomic<float> worldScale{90.0f},resolutionScale{1.0f};
std::atomic<bool> recentre{true},captureRequested{false};
std::atomic<unsigned> backW{0},backH{0};
thread_local unsigned depth=0,calcDepth=0;
thread_local void* viewportNow=nullptr;
thread_local int eyeSign=0;
thread_local W slotNow=0,poseGen=0;
thread_local bool foundView=false,poseValid=false,afwFrame=false;
thread_local vr::HeadPose head{},eyes[2]{};
thread_local float angles[2][4]{};
thread_local unsigned afwNext=0;
float refYaw=0,refPos[3]{};
void settings_save(); // below
struct M4 { float m[4][4]; };
struct Tag { void* viewport; W gen; int sign; bool afw; W slot; };
constexpr unsigned kTags=64;
Tag tags[kTags]{};
SRWLOCK tagLock=SRWLOCK_INIT;
W written=0,read=0;
struct Caller { std::atomic<W> address{0},count{0}; };
Caller callers[16];
struct State { W original; void* right; };
State states[16]{}; // bounded allocations; never reinterpret/copy unknown view-state objects
bool safe(const void* p,void* out,size_t n) {
    __try { memcpy(out,p,n); return true; }
    __except(EXCEPTION_EXECUTE_HANDLER) { return false; }
}
W bits(float v) { W r; memcpy(&r,&v,4); return r; }
float value(W v) { float r; memcpy(&r,&v,4); return r; }
M4 mul(const M4& a,const M4& b) {
    M4 r{};
    for(int i=0;i<4;i++) for(int j=0;j<4;j++) for(int k=0;k<4;k++) r.m[i][j]+=a.m[i][k]*b.m[k][j];
    return r;
}
bool matrices(const M4& v,const M4& p) {
    for(int i=0;i<16;i++) if(!std::isfinite((&v.m[0][0])[i]) || !std::isfinite((&p.m[0][0])[i])) return false;
    // Target-independent UE3 row-vector perspective form; reject UI/orthographic inputs.
    if(std::fabs(v.m[3][3]-1)>0.001f || std::fabs(p.m[2][3]-1)>0.001f ||
       std::fabs(p.m[3][3])>0.001f || p.m[0][0]<0.01f || p.m[1][1]<0.01f) return false;
    for(int i=0;i<3;i++) {
        float norm=0; for(int j=0;j<3;j++) norm+=v.m[j][i]*v.m[j][i];
        if(std::fabs(norm-1)>0.02f) return false;
    }
    return true;
}
void apply_reference(vr::HeadPose& p) {
    float ay=std::sin(-refYaw*0.5f),aw=std::cos(-refYaw*0.5f);
    float x=p.qx,y=p.qy,z=p.qz,w=p.qw;
    p.qw=aw*w-ay*y; p.qx=aw*x+ay*z; p.qy=aw*y+ay*w; p.qz=aw*z-ay*x;
    float c=std::cos(-refYaw),s=std::sin(-refYaw),dx=p.px-refPos[0],dz=p.pz-refPos[2];
    p.px=c*dx+s*dz; p.py-=refPos[1]; p.pz=-s*dx+c*dz;
}
M4 delta(const vr::HeadPose& p) {
    float x=p.qx,y=p.qy,z=p.qz,w=p.qw;
    float R[3][3]={{1-2*(y*y+z*z),2*(x*y-z*w),2*(x*z+y*w)},
                  {2*(x*y+z*w),1-2*(x*x+z*z),2*(y*z-x*w)},
                  {2*(x*z-y*w),2*(y*z+x*w),1-2*(x*x+y*y)}};
    const float flip[3]={1,1,-1};
    M4 d{};
    for(int i=0;i<3;i++) for(int j=0;j<3;j++) d.m[i][j]=R[i][j]*flip[i]*flip[j];
    float uu=worldScale.load(),t[3]={p.px*uu,p.py*uu,-p.pz*uu};
    for(int j=0;j<3;j++) for(int i=0;i<3;i++) d.m[3][j]-=t[i]*d.m[i][j];
    d.m[3][3]=1;
    return d;
}
M4 lens(const M4& original,int e) {
    M4 p=original;
    const float l=std::tan(angles[e][0]),r=std::tan(angles[e][1]);
    const float u=std::tan(angles[e][2]),d=std::tan(angles[e][3]);
    if(vr::asym_fov()) {
        p.m[0][0]=2/(r-l); p.m[2][0]=-(r+l)/(r-l);
        p.m[1][1]=2/(u-d); p.m[2][1]=-(u+d)/(u-d);
    } else {
        p.m[0][0]=1/std::fmax(-l,r); p.m[1][1]=1/std::fmax(-d,u);
        p.m[2][0]=p.m[2][1]=0;
    }
    return p; // engine near/far depth terms preserved
}
void note_caller(W ra) {
    for(auto& c:callers) {
        W expected=0;
        if(c.address.load()==ra || c.address.compare_exchange_strong(expected,ra)) { c.count++; break; }
    }
}
bool push_tag(void* viewport,int sign,bool afw,W gen) {
    AcquireSRWLockExclusive(&tagLock);
    if(written-read>=kTags) {
        ReleaseSRWLockExclusive(&tagLock);
        poisoned.store(true); tagMiss++; set_camera(false); return false;
    }
    slotNow=written;
    tags[written%kTags]={viewport,gen,sign,afw,written};
    ++written;
    ReleaseSRWLockExclusive(&tagLock);
    return true;
}
W __fastcall begin_detour(void* self,void* edx) {
    void* viewport=nullptr;
    safe(static_cast<BYTE*>(self)+4,&viewport,4);
    Tag tag{};
    AcquireSRWLockExclusive(&tagLock);
    if(read!=written && tags[read%kTags].viewport==viewport) tag=tags[read++%kTags];
    else tagMiss++;
    ReleaseSRWLockExclusive(&tagLock);
    vr::sr_set_present_eye(tag.sign);
    vr::sr_set_present_gen(tag.gen);
    vr::sr_set_present_afw(tag.sign!=0 && tag.afw);
    gfx_hud::set_redirect(hudOn.load() && tag.sign!=0);
    gfx_hud::note_eye_pass(tag.sign!=0,tag.sign);
    afw::set_render_slot(tag.slot);
    dlss_bridge::set_render_slot(tag.slot);
    reflect::set_render_pass(begins.load(),tag.sign);
    begins++;
    return beginOriginal(self,edx);
}
bool second_draw(void* self,void* edx,W a1) {
    __try { drawOriginal(self,edx,a1); return true; }
    __except(EXCEPTION_EXECUTE_HANDLER) { return false; }
}
void status() {
    vr::PairProbe pair{}; vr::pair_probe(&pair);
    BVR_LOG("[bo] status: hooks=%d caller=%08X last-view-caller=%08X camera=%d native=%d poison=%d "
        "root=%u second=%u view-edits=%u rejected=%u begin=%u tag-miss=%u presents=%llu pairs=%u abort=%u "
        "actual-size=%ux%u runtime=%s",installed.load(),gameplayCaller.load(),lastViewCaller.load(),
        camera.load(),native.load(),poisoned.load(),draws.load(),seconds.load(),edits.load(),invalid.load(),
        begins.load(),tagMiss.load(),d3d11_hook::present_count(),pair.pairs,pair.aborts,
        backW.load(),backH.load(),vr::session_state_name());
    for(auto& c:callers) if(c.address.load()) BVR_LOG("[bo] root caller RVA %08X: %u",c.address.load(),c.count.load());
    char line[1800]{}; afw::status_line(line,sizeof(line)); BVR_LOG("%s",line);
    dlss_bridge::status_line(line,sizeof(line)); BVR_LOG("%s",line);
    menu_state::status_line(line,sizeof(line)); BVR_LOG("[bo] %s | MAPBLIT2 passes without a scene blit blanked %u",line,gfx_hud::map_blits_provisional());
    const auto hc=gfx_hud::census();
    uint32_t early=0,undone=0; gfx_hud::early_ui_stats(&early,&undone);
    BVR_LOG("[bo] HUD: scene boundaries=%u, UI draws=%u, captured=%u, last-frame UI=%u | EARLYUI %d: passes without "
        "the scene blit caught %u (blit came later %u) | EARLYSTRICT %d: full-screen pictures kept as UI %u, second scene blits %u | MAPBLIT %d: %u",hc.boundaries,hc.hudDraws,hc.redirected,hc.lastFrameHud,
        gfx_hud::early_ui(),early,undone,gfx_hud::early_strict(),gfx_hud::early_kept(),gfx_hud::second_blits(),gfx_hud::map_blit(),gfx_hud::map_blits());
    { uint32_t h=0,hu=0; gfx_hud::map_hold_stats(&h,&hu);
      BVR_LOG("[bo] MAPHOLD %d: map passes replaced %u (untagged %u) | VIEWDEBOUNCE %d | MAPMONO %d: mono map presents kept from the headset %u",
              gfx_hud::map_hold_on(),h,hu,viewDebounce.load()?1:0,vr::map_mono_skip()?1:0,vr::map_mono_skipped()); }
    { uint32_t a=0,b=0,c=0; gfx_hud::map_safe_stats(&a,&b,&c);
      BVR_LOG("[bo] MAPSAFE %d: starts %u (without the eye-pass flag %u), full-screen blits dropped %u",gfx_hud::map_safe(),a,b,c); }
    reflect::status();
    transitions::status();
}
void __fastcall draw_detour(void* self,void* edx,W a1) {
    W ra=W(reinterpret_cast<uintptr_t>(_ReturnAddress())-imageBase);
    if(depth) { drawOriginal(self,edx,a1); return; }
    ++draws; note_caller(ra);
    command::poll_from_game_thread(GetTickCount64());
    if(GetAsyncKeyState(VK_F12)&1) recentre.store(true);
    {   // Meta button held: the runtime recentred its space - take the current head as straight ahead (as City)
        static uint32_t s_rtRecenters=vr::runtime_recenter_count();
        const uint32_t n=vr::runtime_recenter_count();
        if(n!=s_rtRecenters) { s_rtRecenters=n; recentre.store(true); BVR_LOG("[bo] runtime recentre (Meta button) - view recentred"); }
    }
    if(GetAsyncKeyState(VK_F11)&1) set_camera(!camera.load());
    if(GetAsyncKeyState(VK_F2)&1) captureRequested.store(true);
    // Same raw head + both complete eye poses/FOVs, one generation, before either render.
    poseValid=vr::get_head_pose_gen(head,&poseGen);
    if(poseValid) for(int e=0;e<2;e++) poseValid=vr::get_eye_pose_gen(poseGen,e,eyes[e],angles[e]) && poseValid;
    if(poseValid) {
        if(recentre.exchange(false)) {
            refYaw=std::atan2(2*(head.qx*head.qz+head.qw*head.qy),1-2*(head.qx*head.qx+head.qy*head.qy));
            refPos[0]=head.px; refPos[1]=head.py; refPos[2]=head.pz;
            vr::recenter_panel();
        }
        apply_reference(head); apply_reference(eyes[0]); apply_reference(eyes[1]);
    }
    const bool eligible=installed.load() && camera.load() && native.load() && !poisoned.load() &&
        poseValid && gameplayCaller.load()!=0 && ra==gameplayCaller.load();
    // VIEWDEBOUNCE2 (JJ 19:03: map flicker + "reset the centering" again; log 19:03:32.384: camera mode off -> ON in 18 ms
    // with VIEWDEBOUNCE on): a draw root from ANOTHER caller (the map draws one now and then, e.g. RVA 002FDE22) made
    // eligible false and switched camera mode off for one frame. While the gameplay caller drew in the last 300 ms, such a
    // root leaves the camera mode alone (it still renders mono, untagged, as before).
    static ULONGLONG s_lastGameplayRoot=0;
    const ULONGLONG nowR=GetTickCount64();
    if(eligible) s_lastGameplayRoot=nowR;
    // VIEWDEBOUNCE3 (JJ 21:4x: "those same issues are still present"; log 21:40:07.635 / 16.426: camera off -> ON in 17 ms
    // again, the same frames as the two untagged passes): the gameplay caller itself can be ineligible for one frame (no
    // fresh pose generation). While head tracking and native stereo are wanted and not poisoned, any single ineligible root
    // within 300 ms of an eligible one leaves camera mode alone; that frame renders mono (untagged) and on the map MAPMONO
    // keeps it from the headset.
    const bool foreignBlip=!eligible && viewDebounce.load() && camera.load() && native.load() && !poisoned.load() &&
                           nowR-s_lastGameplayRoot<300;
    if(!foreignBlip) vr::set_camera_mode(eligible);
    const bool active=eligible && vr::vr_camera_mode();
    viewportNow=self; ++depth; foundView=false;
    afwFrame=active && afw::on();
    eyeSign=active ? (afwFrame && afwNext ? 1 : -1) : 0;
    // Only the ordinary enqueue branch produces BeginDrawing. Early/special branches remain mono.
    W special=0,disabled=0,movieFlag=0;
    safe(reinterpret_cast<void*>(imageBase+0x14f6208),&disabled,4);
    safe(reinterpret_cast<void*>(imageBase+0x14000e8),&special,4);
    safe(reinterpret_cast<void*>(imageBase+0x13c5b48),&movieFlag,4);
    const bool ordinary=!disabled && (!special || movieFlag);
    if(!ordinary) eyeSign=0;
    if(ordinary && !push_tag(self,eyeSign,afwFrame,poseGen)) eyeSign=0;
    drawOriginal(self,edx,a1);
    if(foundView) lastViewCaller.store(ra);
    // startup movies / pure 2D roots use the room screen. VIEWDEBOUNCE (JJ 2026-10-13: during a map flicker "it actually
    // reset the centering of the VR field of view"; log 18:55:40.5 / 41.0: camera mode off -> ON twice within 20 ms = single
    // map frames with no 3D view): only after the view has been missing for 300 ms, not on one frame. `bo viewdebounce 0|1`.
    static ULONGLONG s_lastView=0;
    const ULONGLONG nowV=GetTickCount64();
    if(foundView) s_lastView=nowV;
    else if(!foreignBlip && (!viewDebounce.load() || nowV-s_lastView>300)) vr::set_camera_mode(false);
    if(active && ordinary && foundView) {
        if(afwFrame) { afw::note_drawn(afwNext); afwNext^=1; }
        else {
            eyeSign=1;
            if(push_tag(self,1,false,poseGen) && second_draw(self,edx,a1)) ++seconds;
            else { poisoned.store(true); set_stereo(false); BVR_LOG("[bo] second render stopped; restart after fault/tag overflow"); }
        }
    }
    eyeSign=0; afwFrame=false; --depth; viewportNow=nullptr;
    {   // RELEASE 0.1.0: a DLSS change made by the mod itself (no DLSS on this PC -> off) is saved, so the launcher's next start
        // draws full size (City saves every change by itself; Origins saved only panel changes).
        static int s_dlssSaved=-1; const int m=dlss_bridge::saved_mode();
        if(s_dlssSaved<0) s_dlssSaved=m; else if(m!=s_dlssSaved) { s_dlssSaved=m; settings_save(); }
    }
    static ULONGLONG previous=0; // RELEASE 0.1.0: the heartbeat every 30 s, every 5 s with test options on
    if(GetTickCount64()-previous>(overlay::show_tests() ? 5000u : 30000u)) { previous=GetTickCount64(); status(); }
}
// MAPREC (2026-10-13): a map visit's record - 10 s of eye pictures + HUD sheets (EYEBURST) and the per-present draw tracer
// (EYB3/EYB4: every leaking eye picture with the draws that put it there). Since RELEASE 0.1.0 only from the TESTS drawer
// or `bo maprec on` (then every map opening / closing records by itself, as in the test builds).
void record_map(bool open) {
    uint32_t a=0,b=0,c=0; gfx_hud::map_safe_stats(&a,&b,&c);
    BVR_LOG("[bo] MAPREC: map %s (MAPSAFE %d: starts %u, without eye pass %u, blits dropped %u) - recording %s",
            open ? "OPEN" : "closed",gfx_hud::map_safe(),a,b,c,open ? "10 s" : "5 s");
    vr::eye_burst::start(open ? 1800 : 900); // ~10 s open, ~5 s after closing
    if(open && !gfx_hud::leak_trace_running()) {
        SYSTEMTIME st; GetLocalTime(&st);
        wchar_t dir[MAX_PATH]{}; swprintf_s(dir,L"%s\\captures",log::data_dir()); CreateDirectoryW(dir,nullptr);
        swprintf_s(dir,L"%s\\captures\\maptrace_%02d%02d%02d",log::data_dir(),st.wHour,st.wMinute,st.wSecond);
        CreateDirectoryW(dir,nullptr);
        gfx_hud::start_leak_trace(dir,10800); // the whole map visit (up to ~60 s)
    }
}
void* __fastcall calc_detour(void* self,void* edx,void* family,float* loc,int32_t* rot,void* viewport,void* drawer) {
    const bool mapNow=menu_state::live() && menu_state::screen_menu_open();
    gfx_hud::set_menu_screen(mapNow); // MS3: MAPBLIT on the game's signal
    {
        static bool s_prevMap=false;
        if(mapNow!=s_prevMap) { s_prevMap=mapNow; if(mapRec.load()) record_map(mapNow); }
    }
    const bool ours=depth && viewport==viewportNow;
    if(ours) ++calcDepth;
    void* out=calcOriginal(self,edx,family,loc,rot,viewport,drawer);
    if(ours) --calcDepth;
    return out;
}
void* __fastcall ctor_detour(void* self,void* edx,A28) {
    const uintptr_t ra=reinterpret_cast<uintptr_t>(_ReturnAddress())-imageBase;
    if(!depth || !calcDepth || ra!=0x3d4eac) return ctorOriginal(self,edx,P28(a14,a15));
    M4 base{},projection{};
    if(!safe(reinterpret_cast<void*>(a14),&base,64) || !safe(reinterpret_cast<void*>(a15),&projection,64) ||
       !matrices(base,projection) || value(a12)<=0 || value(a13)<=0) {
        ++invalid;
        if(eyeSign) { set_camera(false); BVR_LOG("[bo] camera input rejected; returning to headset screen"); }
        return ctorOriginal(self,edx,P28(a14,a15));
    }
    foundView=true;
    if(!eyeSign || !poseValid) return ctorOriginal(self,edx,P28(a14,a15));
    int e=eyeSign<0 ? 0 : 1;
    M4 view=mul(base,delta(eyes[e]));
    M4 proj=lens(projection,e),unjittered=proj;
    unsigned w=backW.load(),h=backH.load();
    if(!w || !h) return ctorOriginal(self,edx,P28(a14,a15));
    a10=bits(0); a11=bits(0); a12=bits(float(w)); a13=bits(float(h));
    float jx=0,jy=0;
    if(dlss_bridge::jitter(draws.load() >> (afwFrame ? 1 : 0),w,h,&jx,&jy)) {
        proj.m[2][0]+=2*jx/w; proj.m[2][1]-=2*jy/h;
    }
    if(eyeSign>0 && a2) {
        bool haveRight=false;
        for(auto& state:states) if(state.original==a2 || !state.original) {
            if(!state.original) {
                state.original=a2;
                state.right=reinterpret_cast<AllocFn>(imageBase+0x5d4920)();
                BVR_LOG("[bo] separate right-eye state original=%08X right=%p",a2,state.right);
            }
            if(state.right) { a2=W(reinterpret_cast<uintptr_t>(state.right)); haveRight=true; }
            break;
        }
        if(!haveRight) {
            poisoned.store(true); set_camera(false);
            BVR_LOG("[bo] independent right-eye history unavailable; returning to screen");
            return ctorOriginal(self,edx,P28(a14,a15));
        }
    }
    ++edits;
    M4 vp=mul(view,proj);
    dlss_bridge::set_pass(slotNow,eyeSign,&vp.m[0][0],&view.m[0][0],jx,jy,w,h);
    if(afwFrame) {
        M4 other=mul(mul(base,delta(eyes[1-e])),lens(projection,1-e));
        M4 vpU=mul(view,unjittered),op=lens(projection,1-e);
        afw::set_pass(slotNow,e,&vp.m[0][0],&vpU.m[0][0],&other.m[0][0],op.m[2][2],op.m[3][2],w,h);
    }
    vr::set_rendered_hfov(2*std::atan(1/proj.m[0][0])*57.29578f);
    vr::publish_gameplay_view(true);
    return ctorOriginal(self,edx,P28(W(reinterpret_cast<uintptr_t>(&view)),W(reinterpret_cast<uintptr_t>(&proj))));
}
struct Seam { W rva; const char* prefix; unsigned relocation; void* detour; void** original; };
bool signature(const Seam& seam) {
    BYTE expected[16]{};
    for(unsigned i=0;i<16;i++) { unsigned v; sscanf_s(seam.prefix+i*2,"%2x",&v); expected[i]=BYTE(v); }
    if(seam.relocation) {
        W v; memcpy(&v,expected+seam.relocation,4); v+=W(imageBase-0x400000);
        memcpy(expected+seam.relocation,&v,4);
    }
    BYTE actual[16]{};
    return safe(reinterpret_cast<void*>(imageBase+seam.rva),actual,16) && !memcmp(actual,expected,16);
}
void put(const wchar_t* path,const wchar_t* section,const wchar_t* key,float v) {
    wchar_t text[40]{}; swprintf_s(text,L"%.2f",v);
    WritePrivateProfileStringW(section,key,text,path);
}
void put_int(const wchar_t* path,const wchar_t* section,const wchar_t* key,int v) {
    wchar_t text[16]{}; swprintf_s(text,L"%d",v);
    WritePrivateProfileStringW(section,key,text,path);
}
float get(const wchar_t* path,const wchar_t* section,const wchar_t* key,float fallback) {
    wchar_t text[64]{}; GetPrivateProfileStringW(section,key,L"",text,64,path);
    return text[0] ? float(wcstod(text,nullptr)) : fallback;
}
void settings_save() {
    wchar_t path[MAX_PATH]{}; swprintf_s(path,L"%s\\settings.ini",log::data_dir());
    put(path,L"view",L"units_per_metre",worldScale.load());
    put(path,L"launch",L"resolution_scale",resolutionScale.load());
    put_int(path,L"launch",L"dlss",dlss_bridge::saved_mode());
    float d,w,u; vr::get_hud_quad(&d,&w,&u);
    put(path,L"hud",L"distance",d); put(path,L"hud",L"width",w); put(path,L"hud",L"height",u);
    // RELEASE 0.1.0 (City panel): the settings City's drawers offer, saved the same way.
    put_int(path,L"hud",L"on_head",vr::hud_head_locked() ? 1 : 0);
    float md=0,mw=0; vr::get_hud_menu_quad(&md,&mw);
    put(path,L"hud",L"menu_distance",md); put(path,L"hud",L"menu_width",mw);
    float sd=0,sw=0; vr::get_screen_quad(&sd,&sw);
    put(path,L"screens",L"distance",sd); put(path,L"screens",L"width",sw);
    float pw=0,pd=0; vr::get_panel_size(&pw,&pd);
    put(path,L"screens",L"panel_width",pw); put(path,L"screens",L"panel_distance",pd);
    put_int(path,L"stereo",L"afw",afw::on() ? 1 : 0); put_int(path,L"stereo",L"afw_mode",afw::mode());
}
void settings_load_panel(const wchar_t* path) {
    vr::set_hud_head_locked(GetPrivateProfileIntW(L"hud",L"on_head",vr::hud_head_locked() ? 1 : 0,path)!=0);
    float md=0,mw=0; vr::get_hud_menu_quad(&md,&mw);
    md=get(path,L"hud",L"menu_distance",md); mw=get(path,L"hud",L"menu_width",mw);
    if(md>=.5f && md<=10 && mw>=.5f && mw<=10) vr::set_hud_menu_quad(md,mw);
    float sd=0,sw=0; vr::get_screen_quad(&sd,&sw);
    sd=get(path,L"screens",L"distance",sd); sw=get(path,L"screens",L"width",sw);
    if(sd>=.5f && sd<=10 && sw>=.5f && sw<=10) vr::set_screen_quad(sd,sw);
    float pw=0,pd=0; vr::get_panel_size(&pw,&pd);
    pw=get(path,L"screens",L"panel_width",pw); pd=get(path,L"screens",L"panel_distance",pd);
    if(pw>=.3f && pw<=3 && pd>=.3f && pd<=4) vr::set_panel_size(pw,pd);
    wchar_t v[16]{};
    swprintf_s(v,L"%d",GetPrivateProfileIntW(L"stereo",L"afw",afw::on() ? 1 : 0,path));
    char a[16]{}; WideCharToMultiByte(CP_ACP,0,v,-1,a,16,nullptr,nullptr); afw::settings_read("afw",a);
    swprintf_s(v,L"%d",GetPrivateProfileIntW(L"stereo",L"afw_mode",afw::mode(),path));
    WideCharToMultiByte(CP_ACP,0,v,-1,a,16,nullptr,nullptr); afw::settings_read("afw_mode",a);
}
}
// RELEASE 0.1.0: the MENUTEST / MAPTRACE / MAPTEST2 timed test threads are removed (MAPTRACE and MAPTEST2 switched MAPBLIT /
// MAPSAFE, both known to stick the map to the face in Origins). The map record stays as one TESTS-drawer button (record_map).
bool install() {
    imageBase=reinterpret_cast<uintptr_t>(GetModuleHandleW(nullptr));
    Seam seams[]={
       {0x27ed90,"6aff687221270164a1000000005081ec",3,reinterpret_cast<void*>(draw_detour),reinterpret_cast<void**>(&drawOriginal)},
       {0x3d40f0,"558bec83e4f06aff6891b7270164a100",9,reinterpret_cast<void*>(calc_detour),reinterpret_cast<void**>(&calcOriginal)},
       {0x5cfdc0,"558bec83e4f06aff68f2a2280164a100",9,reinterpret_cast<void*>(ctor_detour),reinterpret_cast<void**>(&ctorOriginal)},
       {0x265560,"8b49048b018b5060ffd2b808000000c3",0,reinterpret_cast<void*>(begin_detour),reinterpret_cast<void**>(&beginOriginal)},
       {0x5d4920,"6aff681b4f2c0164a1000000005051a1",3,nullptr,nullptr}};
    // Validate all candidates before any mutation, including allocator.
    bool ready=false;
    for(unsigned retry=0;retry<150;retry++) {
        ready=true; for(auto& seam:seams) ready=signature(seam) && ready;
        if(ready) break; Sleep(100);
    }
    if(!ready) { BVR_LOG("[bo] target instruction guard failed; screen remains available, engine edits disabled"); return false; }
    bool ok=true;
    for(auto& seam:seams) if(seam.detour) {
        auto* target=reinterpret_cast<void*>(imageBase+seam.rva);
        if(MH_CreateHook(target,seam.detour,seam.original)!=MH_OK) { ok=false; break; }
        MH_QueueEnableHook(target);
    }
    if(ok) ok=MH_ApplyQueued()==MH_OK;
    if(!ok) {
        for(auto& seam:seams) if(seam.detour) MH_QueueDisableHook(reinterpret_cast<void*>(imageBase+seam.rva));
        MH_ApplyQueued();
        BVR_LOG("[bo] engine hook batch failed; screen only"); return false;
    }
    installed.store(true);
    reflect::install(imageBase); // optional: its own guard; a mismatch leaves stereo unaffected
    menu_state::install();       // MENUSTATE probe: the game's own menu calls, logged by name
    // LIVE 2026-10-11: this Origins root caller produced thousands of native
    // pairs in the Cell Block A checkpoint with no tag misses or pair aborts.
    gameplayCaller.store(0x308607);
    gfx_hud::set_armed(true); gfx_hud::set_redirect(false);
    gfx_hud::set_triangle_boundary(true);
    gfx_hud::set_fx_passthrough(false);
    // City-specific wipe/fade classifiers are NOT Origins evidence; keep them off.
    gfx_hud::set_hide_wipes(false); gfx_hud::set_open_fade(false); gfx_hud::set_load_hold(false);
    gfx_hud::set_replay_enabled(false); gfx_hud::set_black_fill(false); gfx_hud::set_stage_box(false);
    gfx_hud::set_fade_hold(0); gfx_hud::set_wipe_panel_off(false); gfx_hud::set_menu_fade_black(false);
    gfx_hud::set_hud_zones_on(false);
    vr::set_hud_texture_provider(&gfx_hud::texture);
    vr::set_hud_eyes(2); vr::set_screen_room(true);
    // Keep the entire sheet inside the Quest shared binocular zone (~70 deg).
    vr::set_hud_quad(4.0f,4.2f,-0.1f);
    vr::set_hud_menu_quad(4.5f,5.0f);
    vr::set_asym_fov(true); vr::set_eye_tag_rendered(false);
    vr::set_exact_pose_tag(true); vr::sr_use_render_thread_eyes(true);
    vr::set_sr_pair_pacing(true); vr::set_pace_sync(true);
    dlss_bridge::install(); afw::install();
    transitions::install();
    wchar_t settings[MAX_PATH]{},text[64]{};
    swprintf_s(settings,L"%s\\settings.ini",log::data_dir());
    float hd=4,hw=4.2f,hu=-.1f;
    float* vals[]={&hd,&hw,&hu}; const wchar_t* keys[]={L"distance",L"width",L"height"};
    const wchar_t* defaults[]={L"4",L"4.2",L"-0.1"};
    for(int i=0;i<3;i++) { GetPrivateProfileStringW(L"hud",keys[i],defaults[i],text,64,settings); *vals[i]=float(wcstod(text,nullptr)); }
    vr::set_hud_quad(std::clamp(hd,.5f,10.f),std::clamp(hw,.5f,8.f),std::clamp(hu,-1.f,1.f));
    GetPrivateProfileStringW(L"view",L"units_per_metre",L"90",text,64,settings);
    worldScale.store(float(std::clamp(wcstod(text,nullptr),40.0,160.0)));
    GetPrivateProfileStringW(L"launch",L"resolution_scale",L"1",text,64,settings);
    resolutionScale.store(float(std::clamp(wcstod(text,nullptr),.6,1.8)));
    settings_load_panel(settings);
    const wchar_t* cmd=GetCommandLineW();
    auto* outputX=wcsstr(cmd,L"-BOOutputX="); auto* outputY=wcsstr(cmd,L"-BOOutputY=");
    auto* modeArg=wcsstr(cmd,L"-BODLSS=");
    if(outputX && outputY) {
        unsigned ow=wcstoul(outputX+11,nullptr,10),oh=wcstoul(outputY+11,nullptr,10);
        int mode=modeArg ? int(wcstol(modeArg+8,nullptr,10)) : 0;
        if(ow>=512 && oh>=512 && ow<=8192 && oh<=8192 && mode>=0 && mode<=5) {
            dlss_bridge::set_launch(mode,ow,oh); vr::set_output_size(ow,oh);
        }
    }
    input::set_enabled(true);
    vr::set_camera_mode(false); // native intent; gameplay owner selected from THIS game's live census
    set_camera(true);
    BVR_LOG("[bo] native stereo selected by default; live-validated Origins owner RVA 00308607");
    return true;
}
bool hook_live() { return installed.load(); }
void set_camera(bool on) {
    camera.store(on);
    vr::set_camera_mode(on && native.load() && gameplayCaller.load()!=0 && !poisoned.load());
}
void set_stereo(bool on) {
    native.store(on);
    if(on && gameplayCaller.load()==0) gameplayCaller.store(lastViewCaller.load());
    if(on) camera.store(true);
    set_camera(camera.load());
    BVR_LOG("[bo] native %s, owner RVA %08X",on ? "ON" : "off",gameplayCaller.load());
}
void on_present(IDXGISwapChain* sc) {
    startup::tick();
    DXGI_SWAP_CHAIN_DESC d{}; if(SUCCEEDED(sc->GetDesc(&d))) { backW.store(d.BufferDesc.Width); backH.store(d.BufferDesc.Height); }
    if(!captureRequested.exchange(false)) return;
    wchar_t folder[MAX_PATH]{},parent[MAX_PATH]{};
    swprintf_s(parent,L"%s\\captures",log::data_dir()); CreateDirectoryW(parent,nullptr);
    swprintf_s(folder,L"%s\\%llu",parent,GetTickCount64()); CreateDirectoryW(folder,nullptr);
    wchar_t hudPath[MAX_PATH]{}; swprintf_s(hudPath,L"%s\\hud_panel.bmp",folder);
    gfx_hud::save_panel_picture(hudPath);
    ID3D11Device* dev=nullptr;ID3D11DeviceContext* ctx=nullptr;
    if(SUCCEEDED(sc->GetDevice(IID_PPV_ARGS(&dev)))) {
        dev->GetImmediateContext(&ctx);transitions::save_capture(ctx,folder);
        if(ctx) ctx->Release();dev->Release();
    }
    vr::save_eye_pictures(folder);
    status(); BVR_LOG("[bo] capture requested in %ls",folder);
}
bool handle_command(const char* args) {
    if(!strcmp(args,"status")) {status();}
    else if(!strncmp(args,"caller ",7)) {
        W next=!strcmp(args+7,"last") ? lastViewCaller.load() : W(strtoul(args+7,nullptr,16));
        bool known=false; for(auto& c:callers) if(c.address.load()==next && next) known=true;
        if(known) { gameplayCaller.store(next); set_camera(camera.load()); BVR_LOG("[bo] explicitly selected caller RVA %08X",next); }
        else BVR_LOG("[bo] caller rejected: no live root observation for RVA %08X",next);
    }
    else if(!strcmp(args,"stereo on")) set_stereo(true);
    else if(!strcmp(args,"stereo off")) set_stereo(false);
    else if(!strcmp(args,"cam on")) set_camera(true);
    else if(!strcmp(args,"cam off") || !strcmp(args,"screen")) set_camera(false);
    else if(!strcmp(args,"asym on")) vr::set_asym_fov(true);
    else if(!strcmp(args,"asym off")) vr::set_asym_fov(false);
    else if(!strcmp(args,"hud on")) hudOn.store(true);
    else if(!strcmp(args,"hud off")) { hudOn.store(false); gfx_hud::set_redirect(false); }
    else if(!strcmp(args,"capture")) captureRequested.store(true);
    else if(!strcmp(args,"maprecord")) record_map(true);
    else if(!strcmp(args,"mapsafe on")) gfx_hud::set_map_safe(true);
    else if(!strcmp(args,"mapsafe off")) gfx_hud::set_map_safe(false);
    else if(!strcmp(args,"maprec on")) mapRec.store(true);
    else if(!strncmp(args,"mapmono ",8)) vr::set_map_mono_skip(atoi(args+8)!=0);
    else if(!strcmp(args,"maphold on")) gfx_hud::set_map_hold(true);
    else if(!strcmp(args,"maphold off")) gfx_hud::set_map_hold(false);
    else if(!strncmp(args,"viewdebounce ",13)) { viewDebounce.store(atoi(args+13)!=0); BVR_LOG("[bo] VIEWDEBOUNCE %d",viewDebounce.load()?1:0); }
    else if(!strcmp(args,"maprec off")) mapRec.store(false);
    else if(!strncmp(args,"eyeburst",8)) { const int n=args[8] ? atoi(args+8) : 540; vr::eye_burst::start(n); BVR_LOG("[bo] EYEBURST requested: %d presents",n); }
    else if(!strcmp(args,"menuprobe")) menu_state::dump();
    else if(!strcmp(args,"mapblit on")) gfx_hud::set_map_blit(true);
    else if(!strncmp(args,"mapblit src ",12)) gfx_hud::set_map_blit_source(atoi(args+12));
    else if(!strcmp(args,"menureset")) menu_state::clear_stale("bo menureset");
    else if(!strcmp(args,"mapblit off")) gfx_hud::set_map_blit(false);
    else if(!strcmp(args,"earlyui on")) gfx_hud::set_early_ui(true);
    else if(!strcmp(args,"earlyui off")) gfx_hud::set_early_ui(false);
    else if(!strcmp(args,"earlystrict on")) gfx_hud::set_early_strict(true);
    else if(!strcmp(args,"earlystrict off")) gfx_hud::set_early_strict(false);
    else if(!strncmp(args,"hudeyes ",8)) { vr::set_hud_eyes(atoi(args+8)); BVR_LOG("[bo] HUD per-eye copies %d",vr::hud_eyes()); }
    else if(!strcmp(args,"recenter")) recentre.store(true);
    else if(!strncmp(args,"afw ",4)) return afw::command(args+4);
    else if(!strncmp(args,"bats ",5)) return transitions::command(args+5);
    else if(!strncmp(args,"reflect ",8)) return reflect::command(args+8);
    else if(!strncmp(args,"dlss ",5)) { bool ok=dlss_bridge::command(args+5); settings_save(); return ok; }
    else return false;
    return true;
}
// RELEASE 0.1.0 (JJ 2026-10-14: "clean up unneeded options in the panel and make the panel consistent with arkham city"):
// City's layout (batman_city scene.cpp draw_ui, from AKVR's TIDY4/DRAWERS) - status, Recenter / Save capture / Head
// tracking, controller hint, closed drawers of player settings in plain words (PANELWORDS), and everything for testing in
// ONE drawer at the bottom. Dropped from the panel (commands remain): "Show game on a screen" (= head tracking off), "Turn
// 3D on" (the owner is fixed at start), the flat HUD switch (`bo hud off`: one picture in both eyes cannot be fused,
// VR_ESSENTIALS 6), the MAPSAFE switch (`bo mapsafe on`: stuck the map to the face) and the timed MAPTEST2 recorder.
void draw_ui() {
    constexpr float kPi=3.14159265f;
    const ImVec4 kGreen(0.3f,1.0f,0.55f,1.0f),kAmber(1.0f,0.7f,0.25f,1.0f);
    static ULONGLONG s_rateT=0; static W s_rateDraws=0; static float s_fps=0;
    {   const ULONGLONG now=GetTickCount64();
        if(now-s_rateT>=1000) { const W d=draws.load(); if(s_rateT) s_fps=float(d-s_rateDraws)*1000.0f/float(now-s_rateT); s_rateT=now; s_rateDraws=d; } }
    ImGui::TextColored(ImVec4(1.0f,0.85f,0.2f,1.0f),"Batman Origins VR");
    ImGui::SameLine();
    ImGui::TextDisabled("   build: " __DATE__ " " __TIME__);
    if(!installed.load()) {
        ImGui::TextColored(kAmber,"This game version is not supported: the game is shown on a screen in the room.");
        return;
    }
    {
        const bool vrOn=vr::session_live() && vr::vr_camera_mode();
        ImGui::TextColored(vrOn ? kGreen : kAmber,"%s",vrOn ? "VR on" : (vr::session_live() ? "VR connected, not showing yet" : "VR not connected"));
        ImGui::SameLine();
        const bool st=native.load() && !poisoned.load();
        ImGui::TextDisabled("|  %.0f fps  |  %s  |  head tracking %s",s_fps,
                            !st ? "3D off" : (afw::on() ? "3D, one eye drawn per frame (AFW)" : "true 3D, both eyes every frame"),
                            camera.load() ? "on" : "OFF (F11)");
    }
    if(poisoned.load()) ImGui::TextColored(kAmber,"3D switched itself off after an error. Restart the game to get it back.");
    if(ImGui::Button("Recenter  (F12)")) recentre.store(true);
    ImGui::SameLine();
    if(ImGui::Button("Save capture  (F2)")) captureRequested.store(true);
    ImGui::SameLine();
    if(ImGui::Button(camera.load() ? "Head tracking: ON  (F11)" : "Head tracking: OFF  (F11)")) set_camera(!camera.load());
    ImGui::TextDisabled("controller: left stick / d-pad = move, A = select / change, B = back, both stick clicks = close");
    ImGui::Separator();

    ImGui::SetNextItemOpen(true,ImGuiCond_Once);
    if(ImGui::CollapsingHeader("STEREO RENDERING")) {
        const bool was=afw::on();
        const int wasMode=afw::mode();
        afw::draw_ui();
        if(afw::on()!=was || afw::mode()!=wasMode) { if(!native.load()) set_stereo(true); settings_save(); }
    }
    ImGui::SetNextItemOpen(false,ImGuiCond_Once);
    if(ImGui::CollapsingHeader("VIEW")) {
        float scale=worldScale.load();
        if(overlay::SliderStep("world scale  (higher = the world looks smaller)",&scale,40,160,"%.0f")) { worldScale.store(scale); settings_save(); }
    }
    ImGui::SetNextItemOpen(false,ImGuiCond_Once);
    if(ImGui::CollapsingHeader("HUD")) {
        bool hh=vr::hud_head_locked();
        if(ImGui::Checkbox("attach UI to head movement",&hh)) { vr::set_hud_head_locked(hh); settings_save(); }
        // City's HUDDIST controls: the panel is set by its distance and the angle it covers, so moving it nearer or further
        // keeps its size in view (width = 2 x distance x tan(angle / 2)); the height offset scales with the distance too.
        float q[3]; vr::get_hud_quad(&q[0],&q[1],&q[2]);
        float deg=2.0f*std::atan(0.5f*q[1]/q[0])*180.0f/kPi;
        const float d0=q[0];
        bool ch=overlay::SliderStep("HUD distance (m)",&q[0],0.5f,8.0f,"%.2f");
        ch|=overlay::SliderStep("HUD size (degrees of your view)",&deg,30.0f,120.0f,"%.0f");
        float upDeg=std::atan(q[2]/d0)*180.0f/kPi;
        ch|=overlay::SliderStep("HUD height (degrees, + up)",&upDeg,-30.0f,30.0f,"%.0f");
        if(ch) {
            q[1]=2.0f*q[0]*std::tan(0.5f*deg*kPi/180.0f);
            q[2]=q[0]*std::tan(upDeg*kPi/180.0f);
            vr::set_hud_quad(q[0],q[1],q[2]); vr::recenter_panel(); settings_save();
        }
        float md=0,mw=0; vr::get_hud_menu_quad(&md,&mw);
        float mdeg=2.0f*std::atan(0.5f*mw/md)*180.0f/kPi;
        bool mc=overlay::SliderStep("pause / map screen distance (m)",&md,0.5f,8.0f,"%.2f");
        mc|=overlay::SliderStep("pause / map screen size (degrees)",&mdeg,30.0f,120.0f,"%.0f");
        if(mc) { vr::set_hud_menu_quad(md,2.0f*md*std::tan(0.5f*mdeg*kPi/180.0f)); settings_save(); }
    }
    ImGui::SetNextItemOpen(false,ImGuiCond_Once);
    if(ImGui::CollapsingHeader("MENUS AND SCREENS")) {
        transitions::draw_ui();
        float d=0,w=0; vr::get_screen_quad(&d,&w);
        bool ch=overlay::SliderStep("movie / loading screen distance (m)",&d,0.8f,6.0f,"%.2f");
        ch|=overlay::SliderStep("movie / loading screen size (m)",&w,0.8f,6.0f,"%.2f");
        if(ch) { vr::set_screen_quad(d,w); settings_save(); }
        float pw=0,pd=0; vr::get_panel_size(&pw,&pd);
        bool pc=overlay::SliderStep("this menu: distance (m)",&pd,0.6f,3.0f,"%.2f");
        pc|=overlay::SliderStep("this menu: size (m)",&pw,0.5f,2.0f,"%.2f");
        if(pc) { vr::set_panel_size(pw,pd); settings_save(); }
    }
    ImGui::SetNextItemOpen(false,ImGuiCond_Once);
    if(ImGui::CollapsingHeader("VR RESOLUTION")) {
        uint32_t w=0,h=0; vr::recommended_eye_size(&w,&h);
        float s=resolutionScale.load();
        if(overlay::SliderStep("picture size  (x the headset's own size)",&s,.6f,1.8f,"%.2f")) { resolutionScale.store(s); settings_save(); }
        ImGui::TextDisabled("   takes effect the next time the game starts");
        if(w && h) ImGui::TextDisabled("   the headset asks for %ux%u per eye; the game draws %ux%u now",w,h,backW.load(),backH.load());
    }
    ImGui::SetNextItemOpen(false,ImGuiCond_Once);
    if(ImGui::CollapsingHeader("DLSS (SMOOTHER EDGES)")) {
        const int before=dlss_bridge::saved_mode(); dlss_bridge::draw_ui();
        if(before!=dlss_bridge::saved_mode()) settings_save();
    }
    ImGui::SetNextItemOpen(false,ImGuiCond_Once);
    if(ImGui::CollapsingHeader("Keyboard shortcuts")) {
        ImGui::TextDisabled("F10 or both stick clicks - this menu\nF12 - recenter (or hold the headset's menu / Meta button)\n"
                            "F11 - head tracking on/off\nF2 - save a capture (both eyes and the HUD panel)");
    }
    // The one place for tests (JJ 2026-10-14: "keep other tests that might be needed down the line - in a drawer at the
    // bottom of the vr panel"). Nothing in here runs by itself.
    ImGui::SetNextItemOpen(false,ImGuiCond_Once);
    if(ImGui::CollapsingHeader("TESTS  (only when asked - leave alone)")) {
        ImGui::TextDisabled("For finding bugs. Leave these as they are unless a test asks for them.");
        bool st=overlay::show_tests(); if(ImGui::Checkbox("show test numbers in the other drawers",&st)) overlay::set_show_tests(st);
        reflect::draw_ui();
        bool mh=gfx_hud::map_hold_on(); if(ImGui::Checkbox("map flicker fix (leave on)",&mh)) gfx_hud::set_map_hold(mh);
        if(ImGui::Button("Record the map for 10 s  (open the map first)")) record_map(true);
        ImGui::SameLine();
        if(ImGui::Button("Record both eyes for 3 s")) { vr::eye_burst::start(540); BVR_LOG("[bo] EYEBURST requested from the panel: 540 presents"); }
        transitions::draw_test_ui();
        ImGui::TextDisabled("views changed %u, second renders %u, tag misses %u, caller %08X",edits.load(),seconds.load(),
                            tagMiss.load(),gameplayCaller.load());
        uint32_t hold=0,holdU=0; gfx_hud::map_hold_stats(&hold,&holdU);
        ImGui::TextDisabled("map passes replaced %u, mono map frames kept from the headset %u",hold,vr::map_mono_skipped());
        ImGui::TextDisabled("records and captures go to %%LOCALAPPDATA%%\\Batman_Origins\\captures");
    }
}

// Offline contract check against the ACTUAL Origins forwarding detour, not a duplicate wrapper.
namespace {
W testCalls=0,testSum=0,testSelf=0,testEdx=0;
__declspec(noinline) void* __fastcall test_ctor(void* self,void* edx,A28) {
    W args[]={P28(a14,a15)};
    testSelf=W(self); testEdx=W(edx); ++testCalls; testSum=0;
    for(unsigned i=0;i<28;i++) testSum+=(i+1)*args[i];
    return reinterpret_cast<void*>(0x12345678);
}
}
extern "C" __declspec(dllexport) W WINAPI BO_ForwardingSelftest() {
    // Only the foreign-host offline harness may use this. Never change a live game's trampoline.
    wchar_t path[MAX_PATH]{}; GetModuleFileNameW(nullptr,path,MAX_PATH);
    auto* name=wcsrchr(path,L'\\');
    if(_wcsicmp(name ? name+1:path,L"BatmanOrigins.exe")==0 || installed.load()) return 1;
    CtorFn previous=ctorOriginal; ctorOriginal=&test_ctor;
    testCalls=0;
    W expected=0; for(W i=1;i<=28;i++) expected+=i*i;
    for(W n=0;n<4096;n++) {
        void* result=ctor_detour(reinterpret_cast<void*>(0x11112222),reinterpret_cast<void*>(0x33334444),
            1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,16,17,18,19,20,21,22,23,24,25,26,27,28);
        if(result!=reinterpret_cast<void*>(0x12345678) || testSelf!=0x11112222 || testEdx!=0x33334444 ||
           testSum!=expected || testCalls!=n+1) { ctorOriginal=previous; return 2; }
    }
    ctorOriginal=previous;
    M4 original{}; original.m[2][2]=1.00001f; original.m[3][2]=-.1f;
    angles[0][0]=-.8f; angles[0][1]=.9f; angles[0][2]=.85f; angles[0][3]=-.75f;
    vr::set_asym_fov(true);
    M4 p=lens(original,0);
    for(int side=0;side<4;side++) {
        float tangent=std::tan(angles[0][side]);
        float ndc=side<2 ? tangent*p.m[0][0]+p.m[2][0] : tangent*p.m[1][1]+p.m[2][1];
        float want=side==0 || side==3 ? -1.0f:1.0f;
        if(std::fabs(ndc-want)>1e-5f) return 3;
    }
    if(p.m[2][2]!=original.m[2][2] || p.m[3][2]!=original.m[3][2]) return 4;
    return 0;
}
}
