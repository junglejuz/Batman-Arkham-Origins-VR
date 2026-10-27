// Origins adapter. No City/Knight engine offsets or shader identities are used.
// STATIC contracts: retail SHA in RE_NOTES; constructor ret 0x70, matrices words 14/15.
#include "scene.h"
#include "afw.h"
#include "dlss_bridge.h"
#include "first_person.h"
#include "flare_roll.h"
#include "aim_depth.h"
#include "markers.h"
#include "detective.h"
#include "menu_state.h"
#include "reflect.h"
#include "core/vr/eye_burst.h"
#include "core/framework/command.h"
#include "core/gfx/gfx_hud.h"
#include "core/gfx/hud_capture.h"
#include "core/gfx/frame_inspector.h"
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
std::atomic<W> noPose{0}; // STEREOPOSE: draw roots with no complete head + eyes sample (each would render flat)
std::atomic<bool> pauseDirtRemove{true};                     // PAUSEDIRT (below): pause grime drawn with white
std::atomic<bool> loadBurst{true};                           // LOADBURST (on_present): one recording when a load ends
std::atomic<bool> camHold{true};                             // CAMHOLD (ctor_detour): keep the last good game camera
std::atomic<float> entryHoldS{2.5f};                         // ENTRYHOLD: black after a load (s)
std::atomic<bool> hudAim{true};                              // HUDAIM (calc_detour): the HUD rides the head while aiming
std::atomic<ULONGLONG> streakTestAt{0};                      // STREAKTEST (panel): start time, 0 = idle
std::atomic<unsigned> camHeld{0},camCuts{0};
std::atomic<unsigned> pauseDirtRemoved{0},pauseDirtMatches{0};
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
thread_local M4 labelBase{};            // LABELFIX: this pass's drawn camera, before the eye (ctor_detour)
thread_local float labelHead[3]{};      // LABELFIX: this pass's head centre, recentred XR metres
thread_local bool labelPass=false;
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
    flare_roll::note_eye_pass(tag.sign); // FLAREROLL: this eye's tilt for the lens-flare steps
    aim_depth::note_eye_pass(tag.sign); // AIMDEPTH: the left eye's depth under the head's aim
    markers::set_render_slot(tag.slot); // MARKEREYE (OM1): this BeginDrawing's pass
    detective::set_render_pass(tag.sign); // DETVR: this pass's eye
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
        "root=%u second=%u view-edits=%u rejected=%u begin=%u tag-miss=%u no-pose=%u presents=%llu pairs=%u abort=%u "
        "actual-size=%ux%u runtime=%s",installed.load(),gameplayCaller.load(),lastViewCaller.load(),
        camera.load(),native.load(),poisoned.load(),draws.load(),seconds.load(),edits.load(),invalid.load(),
        begins.load(),tagMiss.load(),noPose.load(),d3d11_hook::present_count(),pair.pairs,pair.aborts,
        backW.load(),backH.load(),vr::session_state_name());
    for(auto& c:callers) if(c.address.load()) BVR_LOG("[bo] root caller RVA %08X: %u",c.address.load(),c.count.load());
    BVR_LOG("[bo] PAUSEDIRT remove=%d: grime passes seen %u, drawn without dirt %u | CAMHOLD %d: pairs held %u, real cuts taken %u | FADEHOLD %d",
            pauseDirtRemove.load()?1:0,pauseDirtMatches.load(),pauseDirtRemoved.load(),camHold.load()?1:0,camHeld.load(),camCuts.load(),gfx_hud::fade_hold());
    { char g[200]; flare_roll::status_line(g,sizeof g); BVR_LOG("[bo] %s",g); }
    { char g[200]; aim_depth::status_line(g,sizeof g); BVR_LOG("[bo] %s",g); }
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
    // Same raw head + both complete eye poses/FOVs, one generation, before either render. STEREOPOSE (Asylum HEAD-AIM4,
    // Godoy301): one mailbox read; the old head-then-eyes read could see a new head before its eyes = one flat frame.
    {
        vr::StereoPose stereo{};
        poseValid=vr::get_stereo_pose(stereo);
        if(poseValid) { head=stereo.head; poseGen=stereo.generation; memcpy(eyes,stereo.eyes,sizeof(eyes)); memcpy(angles,stereo.angles,sizeof(angles)); }
        else ++noPose;
    }
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
    // FIRSTPERSON: this pair's eye point, from the first eye's pass (the second eye reuses it). self = the ULocalPlayer.
    if(ours && eyeSign && (eyeSign<0 || afwFrame)) {
        const bool menuUp=menu_state::live() && menu_state::menu_open();
        first_person::update(self,menuUp ? "menu (pause, map, upgrades)" : nullptr);
        // HUDAIM (JJ 2026-10-27: "The HUD should move with my head when holding down the left Trigger"): first and third person.
        // SCANHUD (JJ 2026-10-28): in the evidence scanner too - its centre target is HUD, the scan direction follows the head
        vr::set_hud_aim_head(hudAim.load() && !menuUp && (first_person::left_trigger_now() || detective::scanner_active()));
    }
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
    {   // CAMHOLD (JJ 2026-10-26: pause menu "black frames"; FLIPWATCH 00:09:06 / 00:12:25: for ~1.5 s after a pause-type menu
        // closes the GAME camera alternates between unrelated views - pitch 79.7 / -71.4 / 70.1 ... - pair by pair; playbook
        // b955f224 FAIL-STR-017 -> STR-010 "drop a bad frame"): while a pause-type menu is up or < 3 s after one closed, a pair whose
        // game camera jumps > 25 deg or > 150 UU from the last good one is drawn from the last good camera instead; a new camera
        // that holds for 3 pairs in a row is a real cut and is taken. Counted ("held" / "cuts" in the status line).
        static M4 good{},cand{}; static bool haveGood=false,pairHeld=false; static int pending=0;
        auto fwd=[](const M4& m,float f[3]){ f[0]=m.m[0][2]; f[1]=m.m[1][2]; f[2]=m.m[2][2]; };
        auto org=[](const M4& m,float o[3]){ for(int i=0;i<3;i++) o[i]=-(m.m[3][0]*m.m[i][0]+m.m[3][1]*m.m[i][1]+m.m[3][2]*m.m[i][2]); };
        auto jump=[&](const M4& a,const M4& b){
            float fa[3],fb[3],oa[3],ob[3]; fwd(a,fa); fwd(b,fb); org(a,oa); org(b,ob);
            const float dot=fa[0]*fb[0]+fa[1]*fb[1]+fa[2]*fb[2];
            const float d=std::sqrt((oa[0]-ob[0])*(oa[0]-ob[0])+(oa[1]-ob[1])*(oa[1]-ob[1])+(oa[2]-ob[2])*(oa[2]-ob[2]));
            return dot<0.906f || d>150.0f; // 25 deg
        };
        if(eyeSign<0 || afwFrame) { // decide once per pair, on its first eye
            pairHeld=false;
            const ULONGLONG nowMs=GetTickCount64();
            // CAMHOLD2 (JJ 2026-10-26: "What about the flicker when entering the pause menu?"): always on, not only around menus -
            // a real cut costs 2 pairs (~22 ms) of the old camera; no gameplay camera turns 25 deg or moves 1.5 m in one frame.
            // Each hold logs the menu context, so the next run shows whether holds come on entering, inside or after a menu.
            const bool pauseType=menu_state::live() && menu_state::menu_open() && !menu_state::screen_menu_open();
            const ULONGLONG sinceOpen=nowMs-menu_state::menu_open_at(),sinceClose=menu_state::menu_closed_at() ? nowMs-menu_state::menu_closed_at() : ~0ull;
            if(!haveGood || !camHold.load() || !jump(base,good)) { good=base; haveGood=true; pending=0; }
            else {
                if(pending>0 && !jump(base,cand)) ++pending; else { pending=1; cand=base; }
                if(pending>=3) { good=base; pending=0; camCuts.fetch_add(1,std::memory_order_relaxed); }
                else {
                    pairHeld=true;
                    if(camHeld.fetch_add(1,std::memory_order_relaxed)<60)
                        BVR_LOG("[bo] CAMHOLD: game camera jumped - pair drawn from the last good camera (%s; last menu open %llu ms ago, last close %llu ms ago)",
                                pauseType ? "pause-type menu UP" : menu_state::menu_open() ? "screen menu up" : "no menu",
                                (unsigned long long)sinceOpen,(unsigned long long)sinceClose);
                }
            }
        }
        if(pairHeld) base=good;
    }
    const M4 gameBase=base; // MARKERSTEER: the game camera as the game built it (steered by the head while head aim / look runs)
    {   // FIRSTPERSON: the game camera at his eyes (everything below follows it); HEADAIM gets the pair's head orientation
        const float hq[4]={head.qx,head.qy,head.qz,head.qw};
        first_person::note_game_lens(projection.m[0][0]); // ZOOMFOLLOW: the game's own lens (zoom)
        first_person::apply(base.m,worldScale.load(),hq,eyeSign<0 || afwFrame,eyeSign>0 || afwFrame);
    }
    labelBase=base; labelPass=true; // LABELFIX: this pass's drawn camera and head centre
    for(int j=0;j<3;j++) labelHead[j]=0.5f*((&eyes[0].px)[j]+(&eyes[1].px)[j]);
    int e=eyeSign<0 ? 0 : 1;
    M4 view=mul(base,delta(eyes[e]));
    {   // LEANBACK: in first person the lens never goes back into his head when the player leans back
        float o[3];
        for(int i=0;i<3;i++) o[i]=-(view.m[3][0]*view.m[i][0]+view.m[3][1]*view.m[i][1]+view.m[3][2]*view.m[i][2]);
        if(first_person::clamp_eye(o,worldScale.load()))
            for(int j=0;j<3;j++) view.m[3][j]=-(o[0]*view.m[0][j]+o[1]*view.m[1][j]+o[2]*view.m[2][j]);
    }
    {   // FLAREROLL: this eye's tilt, as an angle in its picture (pixels, y down).
        // HEADROLL (JJ 2026-10-27 night: the streak "is no longer rotating when I roll my head ... but it is rotating when I look left
        // and right"): measured against the GAME world's horizon, a pitched game camera made a sideways look count as a tilt (yawing
        // about a tilted axis rolls the view against that horizon). Only the headset's own tilt against real-world level counts now.
        const auto& q=eyes[e];
        auto rot=[&](float vx,float vy,float vz,float o[3]){ // v + 2 q x (q x v + w v)
            const float tx=q.qy*vz-q.qz*vy+q.qw*vx, ty=q.qz*vx-q.qx*vz+q.qw*vy, tz=q.qx*vy-q.qy*vx+q.qw*vz;
            o[0]=vx+2*(q.qy*tz-q.qz*ty); o[1]=vy+2*(q.qz*tx-q.qx*tz); o[2]=vz+2*(q.qx*ty-q.qy*tx);
        };
        float r[3],u[3],f[3]; rot(1,0,0,r); rot(0,1,0,u); rot(0,0,-1,f); // OpenXR: right +X, up +Y, forward -Z
        float a=0.0f;
        float h[3]={-f[2],0.0f,f[0]}; const float l=std::sqrt(h[0]*h[0]+h[2]*h[2]); // level, across the look
        if(l>0.05f && std::isfinite(l)) {
            h[0]/=l; h[2]/=l;
            float hr=h[0]*r[0]+h[2]*r[2], hu=h[0]*u[0]+h[2]*u[2];
            if(hr<0) { hr=-hr; hu=-hu; }
            a=std::atan2(-hu,hr);
        }
        // LEVELWORLD: the game world's horizon in this view against the game camera's own (FLARESPRITE's first angle, JJ: roll fixed)
        auto horizon=[](const M4& v){
            const float fw[3]={v.m[0][2],v.m[1][2],v.m[2][2]};
            float hh[3]={fw[1],-fw[0],0.0f}; const float hl=std::sqrt(hh[0]*hh[0]+hh[1]*hh[1]);
            if(hl<0.05f) return 0.0f;
            hh[0]/=hl; hh[1]/=hl;
            float hr=hh[0]*v.m[0][0]+hh[1]*v.m[1][0], hu=hh[0]*v.m[0][1]+hh[1]*v.m[1][1];
            if(hr<0) { hr=-hr; hu=-hu; }
            return std::atan2(-hu,hr);
        };
        float w=horizon(view)-horizon(base);
        if(w>3.14159265f) w-=6.2831853f; else if(w<-3.14159265f) w+=6.2831853f;
        flare_roll::set_eye_angles(eyeSign,w,a);
    }
    {   // FLIPWATCH (JJ 2026-10-26, pause / main menu "black flashes": EYEBURST 23:56 caught single stereo pairs looking DOWN at
        // Batman from above while the neighbours looked UP): the game camera's, the head's and the final view's pitch per eye;
        // a jump of more than 25 deg from that eye's previous render is logged with both frames' values.
        // FLIPWATCH2 (LIVE 00:09:06 / 00:12:25: only the GAME camera jumps - 79.7 / -71.4 / 70.1 ... - for ~1.5 s after the pause
        // menu closes, sometimes two views in one generation): also the yaw, the camera position, how many eye views this draw
        // root / generation made, and the identities that could tell the game's views apart (draw root object, view family = a1,
        // view state = a2, the view being built), so the real gameplay view can be told from the extra ones.
        auto pitchOf=[](const M4& m){ return std::asin(std::clamp(m.m[2][2],-1.f,1.f))*57.29578f; }; // forward = column 2, z up
        auto yawOf=[](const M4& m){ return std::atan2(m.m[1][2],m.m[0][2])*57.29578f; };
        const auto& q=eyes[e];
        const float headPitch=std::asin(std::clamp(2*(q.qx*q.qw-q.qy*q.qz),-1.f,1.f))*57.29578f;
        const float basePitch=pitchOf(base),viewPitch=pitchOf(view),baseYaw=yawOf(base);
        float pos[3];
        for(int i=0;i<3;i++) pos[i]=-(base.m[3][0]*base.m[i][0]+base.m[3][1]*base.m[i][1]+base.m[3][2]*base.m[i][2]);
        static float prev[2][4]{}, prevPos[2][3]{}; static W prevGen[2]{}; static bool have[2]{}; static unsigned logged=0;
        static W genNow=0; static unsigned viewsThisGen=0;
        if(poseGen!=genNow) { genNow=poseGen; viewsThisGen=0; }
        ++viewsThisGen;
        float dyaw=std::fabs(baseYaw-prev[e][3]); if(dyaw>180) dyaw=360-dyaw;
        const float dpos=std::sqrt((pos[0]-prevPos[e][0])*(pos[0]-prevPos[e][0])+(pos[1]-prevPos[e][1])*(pos[1]-prevPos[e][1])+
                                   (pos[2]-prevPos[e][2])*(pos[2]-prevPos[e][2]));
        if(have[e] && logged<300 && (std::fabs(viewPitch-prev[e][2])>25 || std::fabs(basePitch-prev[e][0])>25 ||
                                     std::fabs(headPitch-prev[e][1])>25 || dyaw>25 || dpos>150 || viewsThisGen>2)) {
            ++logged;
            BVR_LOG("[bo] FLIPWATCH eye %c gen %u (prev %u) view %u of this gen: game camera pitch %.1f (was %.1f) yaw %.1f (was %.1f) "
                    "pos %.0f/%.0f/%.0f (moved %.0f), head pitch %.1f, view pitch %.1f | root %p family %08X state %08X view %p draws %u%s",
                    e ? 'R' : 'L',poseGen,prevGen[e],viewsThisGen,basePitch,prev[e][0],baseYaw,prev[e][3],pos[0],pos[1],pos[2],dpos,
                    headPitch,viewPitch,viewportNow,a1,a2,self,draws.load(),menu_state::menu_open() ? ", menu open" : "");
        }
        prev[e][0]=basePitch; prev[e][1]=headPitch; prev[e][2]=viewPitch; prev[e][3]=baseYaw;
        for(int i=0;i<3;i++) prevPos[e][i]=pos[i];
        prevGen[e]=poseGen; have[e]=true;
    }
    M4 proj=lens(projection,e),unjittered=proj;
    unsigned w=backW.load(),h=backH.load();
    if(!w || !h) return ctorOriginal(self,edx,P28(a14,a15));
    a10=bits(0); a11=bits(0); a12=bits(float(w)); a13=bits(float(h));
    {   // FACEGAME: this eye's lens + the game camera's right / up in this eye's view space (streak cards face the game camera)
        const float lensT[4]={proj.m[0][0],proj.m[1][1],proj.m[2][0],proj.m[2][1]};
        float gr[3],gu[3];
        for(int a=0;a<3;a++) {
            gr[a]=base.m[0][0]*view.m[0][a]+base.m[1][0]*view.m[1][a]+base.m[2][0]*view.m[2][a];
            gu[a]=base.m[0][1]*view.m[0][a]+base.m[1][1]*view.m[1][a]+base.m[2][1]*view.m[2][a];
        }
        flare_roll::set_eye_face(eyeSign,lensT,gr,gu);
    }
    {   // AIMDEPTH: where this eye's centre ray lands in its picture, the lens' depth terms, the eye's world origin + forward
        const float px=(proj.m[2][0]+1)*0.5f*float(w),py=(1-proj.m[2][1])*0.5f*float(h);
        float o[3],f[3];
        for(int i=0;i<3;i++) { o[i]=-(view.m[3][0]*view.m[i][0]+view.m[3][1]*view.m[i][1]+view.m[3][2]*view.m[i][2]); f[i]=view.m[i][2]; }
        aim_depth::set_eye(eyeSign,px,py,proj.m[2][2],proj.m[3][2],o,f);
    }
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
    void* built=ctorOriginal(self,edx,P28(W(reinterpret_cast<uintptr_t>(&view)),W(reinterpret_cast<uintptr_t>(&proj))));
    {   // MARKEREYE (OM1, Batman City markers.cpp): this pass's view x lens, and the head-free marker reference - the GAME camera's
        // rotation (MARKERSTEER: steered by the head while head aim / look runs) at the drawn position, through this eye's lens made
        // symmetric (the HUD script lays markers out in a centred picture).
        M4 refView=base; float bo[3];
        for(int i=0;i<3;i++) bo[i]=-(base.m[3][0]*base.m[i][0]+base.m[3][1]*base.m[i][1]+base.m[3][2]*base.m[i][2]);
        for(int i=0;i<3;i++) for(int j=0;j<3;j++) refView.m[i][j]=gameBase.m[i][j];
        for(int j=0;j<3;j++) refView.m[3][j]=-(bo[0]*refView.m[0][j]+bo[1]*refView.m[1][j]+bo[2]*refView.m[2][j]);
        M4 refLens=unjittered; refLens.m[2][0]=refLens.m[2][1]=0;
        const M4 refVp=mul(refView,refLens);
        markers::set_pass(slotNow,eyeSign,draws.load(),&vp.m[0][0],&refVp.m[0][0]);
    }
    return built;
}
// MARKEREYE (OM1): the HUD script's Canvas.Project - FSceneView project RVA 0x5BB7C0 (thiscall: FVector4* out, const FVector* in;
// ret 8; out = x/w, y/w, z/w, w through view+0x250), called from UCanvas::execProject (return RVA 0x4F09B2). Found by City's code shape
// (City 0x972FB0 / 0x8FDFD2, the only match in the image; STATIC 2026-10-27 night).
// LABELFIX (Batman Asylum A2f, JJ "Looks great" there; JJ 2026-10-27 night for Origins: "the pointers that are attached to world
// objects like enemies and show information about them or labels on the HUD are not fixed to what they should be pointing to ... the
// vital stats for game characters is the same issue"). STATIC: the HUD's actor-callout code projects each target ONCE and builds its
// label from that point - Origins 0xCCDBD7 (return RVA 0x8CDBDC) and the same function's centre-distance call 0xCCDD2D (return RVA
// 0x8CDD32); Asylum 0x158A366 and City 0xC57D0E have the same shape (depth <= 1, push the actor, x * 0.5, -y * 0.5). That point comes
// from this eye's head-tracked view, but the HUD sheet is ONE room-fixed panel. Corrected before the label is built: the point becomes
// where the line from the head centre to the target crosses the HUD panel, in the panel's picture coordinates - the same for both
// eyes; z / w kept. Behind / off-screen / no panel (HUD riding the head, menus) = the game's own value.
std::atomic<bool> labelFix{true};
std::atomic<unsigned> labelFixed{0},labelKept{0};
// LABELFIX: the point where the line from the head centre to the target crosses the panel (corners top-left, top-right,
// bottom-left), in the panel's picture coordinates (-1..1, y up). Asylum markers9.cpp plane_target.
bool label_on_panel(const float target[3],const float head[3],const float c[3][3],float xy[2]) {
    float right[3],down[3],ray[3];
    for(int j=0;j<3;j++) { right[j]=c[1][j]-c[0][j]; down[j]=c[2][j]-c[0][j]; ray[j]=target[j]-head[j]; }
    const float n[3]={right[1]*down[2]-right[2]*down[1],right[2]*down[0]-right[0]*down[2],right[0]*down[1]-right[1]*down[0]};
    float num=0,den=0,rr=0,dd=0;
    for(int j=0;j<3;j++) { num+=(c[0][j]-head[j])*n[j]; den+=ray[j]*n[j]; rr+=right[j]*right[j]; dd+=down[j]*down[j]; }
    if(std::fabs(den)<1e-6f || rr<1e-6f || dd<1e-6f) return false;
    const float t=num/den;
    if(!std::isfinite(t) || t<=0) return false;
    float u=0,v=0;
    for(int j=0;j<3;j++) { const float o=head[j]+t*ray[j]-c[0][j]; u+=o*right[j]/rr; v+=o*down[j]/dd; }
    xy[0]=2*u-1; xy[1]=1-2*v;
    return std::isfinite(xy[0]) && std::isfinite(xy[1]);
}
void label_fix(const float* in,float* out) {
    if(!labelFix.load(std::memory_order_relaxed) || !labelPass || !eyeSign) return;
    float o[4]; memcpy(o,out,16);
    if(!(o[3]>0.001f) || std::fabs(o[0])>1.1f || std::fabs(o[1])>1.1f) { labelKept.fetch_add(1,std::memory_order_relaxed); return; }
    float c[3][3];
    if(!vr::get_hud_plane(c)) { labelKept.fetch_add(1,std::memory_order_relaxed); return; }
    for(auto& p:c) { vr::HeadPose h{}; h.qw=1; h.px=p[0]; h.py=p[1]; h.pz=p[2]; apply_reference(h); p[0]=h.px; p[1]=h.py; p[2]=h.pz; }
    const float uu=worldScale.load();
    float v[3];
    for(int j=0;j<3;j++) v[j]=in[0]*labelBase.m[0][j]+in[1]*labelBase.m[1][j]+in[2]*labelBase.m[2][j]+labelBase.m[3][j];
    const float target[3]={v[0]/uu,v[1]/uu,-v[2]/uu}; // game view space -> recentred XR (x right, y up, -z forward)
    float xy[2];
    if(!label_on_panel(target,labelHead,c,xy)) { labelKept.fetch_add(1,std::memory_order_relaxed); return; }
    if(labelFixed.fetch_add(1,std::memory_order_relaxed)<6)
        BVR_LOG("[bo] LABELFIX: target %.0f/%.0f/%.0f eye %c game point %.3f/%.3f -> on the HUD panel %.3f/%.3f",in[0],in[1],in[2],
                eyeSign<0 ? 'L' : 'R',o[0],o[1],xy[0],xy[1]);
    out[0]=xy[0]; out[1]=xy[1];
}
using ProjFn=float*(__fastcall*)(void*,void*,float*,const float*);
ProjFn projOriginal=nullptr;
float* __fastcall proj_detour(void* view,void* edx,float* out,const float* in) {
    float* r=projOriginal(view,edx,out,in);
    const uintptr_t ra=reinterpret_cast<uintptr_t>(_ReturnAddress());
    if(ra==imageBase+0x4f09b2 && out && in)
        markers::on_project(eyeSign,draws.load(),in,out);
    else if((ra==imageBase+0x8cdbdc || ra==imageBase+0x8cdd32) && out && in)
        label_fix(in,out); // LABELFIX
    return r;
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
    put_int(path,L"hud",L"on_head_aiming",hudAim.load() ? 1 : 0);
    float md=0,mw=0; vr::get_hud_menu_quad(&md,&mw);
    put(path,L"hud",L"menu_distance",md); put(path,L"hud",L"menu_width",mw);
    float sd=0,sw=0; vr::get_screen_quad(&sd,&sw);
    put(path,L"screens",L"distance",sd); put(path,L"screens",L"width",sw);
    put(path,L"screens",L"entry_hold_s",entryHoldS.load());
    float pw=0,pd=0; vr::get_panel_size(&pw,&pd);
    put(path,L"screens",L"panel_width",pw); put(path,L"screens",L"panel_distance",pd);
    put_int(path,L"stereo",L"afw",afw::on() ? 1 : 0); put_int(path,L"stereo",L"afw_mode",afw::mode());
    put_int(path,L"hud",L"labels_point_at_target",labelFix.load() ? 1 : 0); // LABELFIX
    put_int(path,L"hud",L"detective_overlay_fused",detective::enabled() ? 1 : 0); // DETVR2
    {   // MARKEREYE (OM1): markers.cpp's own key=value lines go into [markers]
        std::string s; markers::settings_write(s);
        size_t a=0;
        while(a<s.size()) {
            size_t b=s.find('\n',a); if(b==std::string::npos) b=s.size();
            const std::string line=s.substr(a,b-a); a=b+1;
            const size_t eq=line.find('='); if(eq==std::string::npos) continue;
            wchar_t k[64]{},v[64]{};
            MultiByteToWideChar(CP_UTF8,0,line.substr(0,eq).c_str(),-1,k,64);
            MultiByteToWideChar(CP_UTF8,0,line.substr(eq+1).c_str(),-1,v,64);
            WritePrivateProfileStringW(L"markers",k,v,path);
        }
    }
}
void settings_load_panel(const wchar_t* path) {
    vr::set_hud_head_locked(GetPrivateProfileIntW(L"hud",L"on_head",vr::hud_head_locked() ? 1 : 0,path)!=0);
    hudAim.store(GetPrivateProfileIntW(L"hud",L"on_head_aiming",1,path)!=0);
    labelFix.store(GetPrivateProfileIntW(L"hud",L"labels_point_at_target",1,path)!=0); // LABELFIX
    detective::set_enabled(GetPrivateProfileIntW(L"hud",L"detective_overlay_fused",1,path)!=0); // DETVR2
    float md=0,mw=0; vr::get_hud_menu_quad(&md,&mw);
    md=get(path,L"hud",L"menu_distance",md); mw=get(path,L"hud",L"menu_width",mw);
    if(md>=.5f && md<=10 && mw>=.5f && mw<=10) vr::set_hud_menu_quad(md,mw);
    float sd=0,sw=0; vr::get_screen_quad(&sd,&sw);
    sd=get(path,L"screens",L"distance",sd); sw=get(path,L"screens",L"width",sw);
    { const float eh=get(path,L"screens",L"entry_hold_s",2.5f); if(eh>=0 && eh<=6) entryHoldS.store(eh); }
    if(sd>=.5f && sd<=10 && sw>=.5f && sw<=10) vr::set_screen_quad(sd,sw);
    float pw=0,pd=0; vr::get_panel_size(&pw,&pd);
    pw=get(path,L"screens",L"panel_width",pw); pd=get(path,L"screens",L"panel_distance",pd);
    if(pw>=.3f && pw<=3 && pd>=.3f && pd<=4) vr::set_panel_size(pw,pd);
    wchar_t v[16]{};
    swprintf_s(v,L"%d",GetPrivateProfileIntW(L"stereo",L"afw",afw::on() ? 1 : 0,path));
    char a[16]{}; WideCharToMultiByte(CP_ACP,0,v,-1,a,16,nullptr,nullptr); afw::settings_read("afw",a);
    swprintf_s(v,L"%d",GetPrivateProfileIntW(L"stereo",L"afw_mode",afw::mode(),path));
    WideCharToMultiByte(CP_ACP,0,v,-1,a,16,nullptr,nullptr); afw::settings_read("afw_mode",a);
    {   // MARKEREYE (OM1): [markers]
        static wchar_t sec[4096]; const DWORD n=GetPrivateProfileSectionW(L"markers",sec,4096,path);
        for(const wchar_t* p=sec; p<sec+n && *p; p+=wcslen(p)+1) {
            char line[160]{}; WideCharToMultiByte(CP_UTF8,0,p,-1,line,160,nullptr,nullptr);
            if(char* eq=strchr(line,'=')) { *eq=0; markers::settings_read(line,eq+1); }
        }
    }
}
// PAUSEDIRT (JJ 2026-10-26: "there is a dirt layer HUD element that's stuck to your face that should probably be removed from the
// pause menu"): City's grime pass (batman_city room_overlay.cpp on_pause_dirt: full-screen Draw(3), t0 4x4, t1 the fp16 scene copy
// >= 1000 wide, t2 a square dirt picture >= 512) is redrawn with plain white in place of the dirt (the pass multiplies by it, so
// white = no grime). Origins' pass is not yet proven to match: while a pause-type menu (pause or main menu chain) is up, each
// distinct full-screen draw's pictures are logged once ("[bo] PAUSEDIRT census"). `bo pausedirt on|off` (on = removed).
ID3D11ShaderResourceView* whiteSrv=nullptr;
bool ensure_white(ID3D11DeviceContext* ctx) {
    if(whiteSrv) return true;
    ID3D11Device* dev=nullptr; ctx->GetDevice(&dev); if(!dev) return false;
    uint32_t px[16]; for(auto& p:px) p=0xFFFFFFFFu;
    D3D11_TEXTURE2D_DESC d{}; d.Width=d.Height=4; d.MipLevels=d.ArraySize=1; d.Format=DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;
    d.SampleDesc.Count=1; d.Usage=D3D11_USAGE_IMMUTABLE; d.BindFlags=D3D11_BIND_SHADER_RESOURCE;
    D3D11_SUBRESOURCE_DATA init{px,16,0}; ID3D11Texture2D* t=nullptr;
    const bool ok=SUCCEEDED(dev->CreateTexture2D(&d,&init,&t)) && SUCCEEDED(dev->CreateShaderResourceView(t,nullptr,&whiteSrv));
    if(t) t->Release(); dev->Release(); return ok;
}
bool tex_desc(ID3D11ShaderResourceView* v,D3D11_TEXTURE2D_DESC* d) {
    if(!v) return false;
    ID3D11Resource* r=nullptr; v->GetResource(&r); if(!r) return false;
    ID3D11Texture2D* t=nullptr; const bool ok=SUCCEEDED(r->QueryInterface(IID_PPV_ARGS(&t)));
    if(ok) { t->GetDesc(d); t->Release(); }
    r->Release(); return ok;
}
void draw_after_plain(ID3D11DeviceContext* ctx) { detective::after_draw(ctx); flare_roll::after_draw(ctx); } // DETVR + FLAREROLL
bool pause_draw_skip(ID3D11DeviceContext* ctx,unsigned vertexCount) {
    if(flare_roll::before_draw(ctx,vertexCount)) return true; // FLAREROLL: flare pieces drawn level (dropped only by its TESTS hide switch)
    aim_depth::before_draw(ctx,vertexCount); // AIMDEPTH: at the tone-map step (never drops the draw)
    detective::before_draw(ctx,vertexCount); // DETVR: records / swaps the detective screen material (never drops the draw)
    if(vertexCount!=3 || !menu_state::menu_open() || menu_state::screen_menu_open()) return false;
    ID3D11ShaderResourceView* srvs[4]{}; ctx->PSGetShaderResources(0,4,srvs);
    D3D11_TEXTURE2D_DESC t[4]{}; bool has[4]{};
    for(int i=0;i<4;++i) has[i]=tex_desc(srvs[i],&t[i]);
    {   // census: one line per distinct picture set (bounded)
        static uint64_t seen[48]{}; static unsigned nSeen=0;
        uint64_t h=1469598103934665603ull;
        for(int i=0;i<4;++i) { const uint64_t k=has[i] ? (uint64_t(t[i].Width)<<40)^(uint64_t(t[i].Height)<<16)^t[i].Format : 0; h=(h^k)*1099511628211ull; }
        bool known=false; for(unsigned i=0;i<nSeen;++i) if(seen[i]==h) known=true;
        if(!known && nSeen<48) {
            seen[nSeen++]=h; char s[4][40]{};
            for(int i=0;i<4;++i) if(has[i]) snprintf(s[i],sizeof s[i],"%ux%u fmt %d",t[i].Width,t[i].Height,int(t[i].Format)); else strcpy_s(s[i],"-");
            BVR_LOG("[bo] PAUSEDIRT census (%s menu): full-screen draw t0 %s | t1 %s | t2 %s | t3 %s",
                    menu_state::screen_menu_open() ? "screen" : "pause-type",s[0],s[1],s[2],s[3]);
        }
    }
    // LIVE 23:55:26 census: Origins' pass = t0 4x4 | t1 1328x1424 fmt 10 (fp16 scene) | t2 256x256 fmt 72 (BC1 sRGB dirt), so
    // City's >= 512 dirt rule missed it; >= 128 square keeps the 4x4 + fp16-scene shape as the real discriminator.
    const bool match=has[0] && has[1] && has[2] && t[0].Width==4 && t[0].Height==4 && t[1].Width>=1000 &&
                     t[1].Format==DXGI_FORMAT_R16G16B16A16_FLOAT && t[2].Width>=128 && t[2].Width==t[2].Height;
    bool handled=false;
    if(match) {
        if(pauseDirtMatches.fetch_add(1)==0) BVR_LOG("[bo] PAUSEDIRT: the pause grime pass found (dirt %ux%u fmt %d), %s",t[2].Width,t[2].Height,int(t[2].Format),pauseDirtRemove.load() ? "drawn with white = no dirt" : "left as is (switch off)");
        if(pauseDirtRemove.load() && ensure_white(ctx)) {
            ctx->PSSetShaderResources(2,1,&whiteSrv); ctx->Draw(3,0); ctx->PSSetShaderResources(2,1,&srvs[2]);
            handled=true; pauseDirtRemoved.fetch_add(1,std::memory_order_relaxed);
        }
    }
    for(auto* s:srvs) if(s) s->Release();
    return handled;
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
    {   // MARKEREYE (OM1): optional, its own guard
        const Seam proj{0x5bb7c0,"8b442408f30f1020f30f106804f30f10",0,reinterpret_cast<void*>(proj_detour),reinterpret_cast<void**>(&projOriginal)};
        auto* target=reinterpret_cast<void*>(imageBase+proj.rva);
        if(signature(proj) && MH_CreateHook(target,proj.detour,proj.original)==MH_OK && MH_EnableHook(target)==MH_OK) {
            gfx_hud::set_marker_fn(&markers::on_hud_draw,&markers::after_hud_draw);
            frame_inspector::set_cb_shadow(true); // MARKEREYE reads each HUD piece's placement
            BVR_LOG("[bo] MARKEREYE: Canvas.Project hooked (RVA 0x5BB7C0) - grapple markers go onto their objects");
        } else BVR_LOG("[bo] MARKEREYE: the projection guard failed - markers stay on the HUD panel");
    }
    frame_inspector::set_draw_skip(&pause_draw_skip); // PAUSEDIRT
    frame_inspector::set_copy_tap(&flare_roll::on_copy); // FLAREROLL: copies of level pictures stay level
    frame_inspector::set_draw_after(&draw_after_plain); // FLAREROLL DEPTHOFF: a detached depth buffer goes back; DETVR: the game's shader back
    frame_inspector::set_draw_indexed_hook(&flare_roll::before_draw_indexed); // PIECECENSUS: indexed glow draws
    frame_inspector::set_draw_indexed_after(&flare_roll::after_draw);
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
    // FADEHOLD on (2026-10-26, JJ's black square after loads; `bo fadehold 0|1`); the other City wipe/fade rules stay off.
    gfx_hud::set_fade_hold(1); gfx_hud::set_wipe_panel_off(false); gfx_hud::set_menu_fade_black(false);
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
    flare_roll::install(); // FLARESPRITE: note every vertex shader the game creates (before its device exists)
    detective::install();  // DETVR: record the detective screen shader candidates (before the game's device exists)
    flare_roll::settings_load(settings);
    first_person::install(imageBase,settings);
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
    {   // LOADBURST (JJ 2026-10-26: "when first entering gameplay from the load screen, a black square appears in the field of view"):
        // once per launch, the first time the game draws a 3D scene again after >= 3 s without one (a load / its video), record
        // ~8 s of both eyes + the HUD sheet (EYEBURST, ~400 MB). `bo loadburst on|off`.
        static bool wasWorld=true; static ULONGLONG worldOffSince=0;
        const bool world=hud::world_pass(); const ULONGLONG now=GetTickCount64();
        if(!world && wasWorld) worldOffSince=now;
        // FADEMID (LIVE 19:24:03-06: the load into gameplay was 2967 ms without a 3D scene - just under the old 3 s, no fade watch)
        if(world && !wasWorld && worldOffSince && now-worldOffSince>=1500) {
            // FADEHOLD (City BATMAN_CITY_PLAN "FADEHOLD", JJ 2026-10-26 Origins: "when first entering gameplay from the load screen,
            // a black square appears in the field of view"; LOADBURST 00:18:03: not in the eye pictures = on the HUD panel): the
            // game's fade-in from black on the HUD stage. gfx_hud holds the panel back while 5 probe points are dark + covered.
            gfx_hud::note_movie_ended();
            // ENTRYHOLD (JJ 2026-10-27: "when entering gameplay, hold the loading screen for another 2 or 3 seconds to avoid showing
            // game elements loading in"): only after a real loading screen (the game's own full-screen movie flag was TRUE during the
            // gap - not after the map or a menu), the eyes stay black and the HUD hidden for the set time.
            const ULONGLONG mv=menu_state::movie_seen_at(); const float hs=entryHoldS.load();
            if(hs>0.05f && mv && mv+1000>=worldOffSince) {
                transitions::hold_black(unsigned(hs*1000)); gfx_hud::hold_panel(unsigned(hs*1000));
                BVR_LOG("[bo] ENTRYHOLD: after a loading screen - black for %.1f s while the world loads in",hs);
            }
            BVR_LOG("[bo] 3D scene back after %llu ms without one (load / video ended)%s",now-worldOffSince,
                    gfx_hud::fade_hold() ? " - fade-in box watch for 3 s" : "");
            if(loadBurst.exchange(false)) {
                vr::eye_burst::start(900);
                BVR_LOG("[bo] LOADBURST: recording both eyes + HUD sheet (900 presents)");
            }
        }
        wasWorld=world;
    }
    if(const ULONGLONG t0=streakTestAt.load()) { // STREAKTEST: dumps at 6 s (head level) and 18 s (head tilted), each with the head's roll
        static ULONGLONG s_run=0; static int s_step=0;
        if(s_run!=t0) { s_run=t0; s_step=0; }
        const ULONGLONG el=GetTickCount64()-t0;
        auto roll=[]{ vr::StereoPose p{}; if(!vr::get_stereo_pose(p)) return 999.0f;
                      const auto& q=p.head; return std::atan2(2*(q.qx*q.qy+q.qw*q.qz),1-2*(q.qx*q.qx+q.qz*q.qz))*57.29578f; };
        if(s_step==0 && el>=6000) { s_step=1; BVR_LOG("[bo] STREAKTEST dump 1 (head level): head roll %.1f deg",roll()); frame_inspector::arm(3,2); }
        else if(s_step==1 && el>=18000) { s_step=2; BVR_LOG("[bo] STREAKTEST dump 2 (head tilted): head roll %.1f deg",roll()); frame_inspector::arm(3,2); }
        else if(s_step==2 && el>=26000) { s_step=3; streakTestAt.store(0); BVR_LOG("[bo] STREAKTEST done"); }
    }
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
    else if(!strncmp(args,"flareroll ",10)) { if(!flare_roll::command(args+10)) BVR_LOG("[bo] usage: bo flareroll on|off"); }
    else if(!strcmp(args,"fadehold 1")) gfx_hud::set_fade_hold(1);
    else if(!strcmp(args,"fadehold 0")) gfx_hud::set_fade_hold(0);
    else if(!strcmp(args,"camhold on")) { camHold.store(true); BVR_LOG("[bo] CAMHOLD on"); }
    else if(!strcmp(args,"camhold off")) { camHold.store(false); BVR_LOG("[bo] CAMHOLD off"); }
    else if(!strcmp(args,"loadburst on")) { loadBurst.store(true); BVR_LOG("[bo] LOADBURST armed for the next load"); }
    else if(!strcmp(args,"loadburst off")) loadBurst.store(false);
    else if(!strcmp(args,"pausedirt on")) { pauseDirtRemove.store(true); BVR_LOG("[bo] PAUSEDIRT: pause dirt removed"); }
    else if(!strcmp(args,"pausedirt off")) { pauseDirtRemove.store(false); BVR_LOG("[bo] PAUSEDIRT: pause dirt shown (game's own)"); }
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
    else if(!strncmp(args,"fp ",3)) return first_person::command(args+3);
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
    first_person::draw_ui(); // FIRSTPERSON: at the top, it changes the whole view (City)

    ImGui::SetNextItemOpen(true,ImGuiCond_Once);
    if(ImGui::CollapsingHeader("STEREO RENDERING")) {
        const bool was=afw::on();
        const int wasMode=afw::mode();
        afw::draw_ui();
        if(afw::on()!=was || afw::mode()!=wasMode) { if(!native.load()) set_stereo(true); settings_save(); }
    }
    ImGui::SetNextItemOpen(false,ImGuiCond_Once);
    if(ImGui::CollapsingHeader("VIEW")) {
        first_person::draw_camera_ui(); // CAMPOS (AKVR "camera position")
        float scale=worldScale.load();
        if(overlay::SliderStep("world scale  (higher = the world looks smaller)",&scale,40,160,"%.0f")) { worldScale.store(scale); settings_save(); }
    }
    // PANELTIDY (JJ 2026-10-27: "Remove the unnecessary things from the VR panel, including the lens flare stuff"): the lens-flare cards
    // always face the game camera (FLATCARDS / FACEGAME, JJ "Finally, it's fixed") - no drawer; `bo flareroll on|off` remains.
    ImGui::SetNextItemOpen(false,ImGuiCond_Once);
    if(ImGui::CollapsingHeader("HUD")) {
        bool hh=vr::hud_head_locked();
        if(ImGui::Checkbox("attach UI to head movement",&hh)) { vr::set_hud_head_locked(hh); settings_save(); }
        if(!hh) {
            bool ha=hudAim.load();
            if(ImGui::Checkbox("   HUD follows your head while you hold the left trigger (aiming)",&ha)) { hudAim.store(ha); settings_save(); }
        }
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
        {   // MARKEREYE (OM1): the grapple markers on their objects (City's markers drawer)
            std::string before; markers::settings_write(before);
            markers::draw_ui();
            bool lf=labelFix.load(); // LABELFIX
            if(ImGui::Checkbox("labels point at their target (enemy info, detective vitals)",&lf)) { labelFix.store(lf); before.clear(); }
            if(detective::draw_ui()) before.clear(); // DETVR2
            if(overlay::show_tests()) ImGui::TextDisabled("   labels placed on the HUD panel %u, left as the game drew them %u",labelFixed.load(),labelKept.load());
            std::string after; markers::settings_write(after);
            if(after!=before) settings_save();
        }
    }
    ImGui::SetNextItemOpen(false,ImGuiCond_Once);
    if(ImGui::CollapsingHeader("MENUS AND SCREENS")) {
        transitions::draw_ui();
        float eh=entryHoldS.load();
        if(overlay::SliderStep("black after a loading screen (s) - hides the world loading in",&eh,0.0f,6.0f,"%.1f")) { entryHoldS.store(eh); settings_save(); }
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
