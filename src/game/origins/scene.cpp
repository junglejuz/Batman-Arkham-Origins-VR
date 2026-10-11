// Origins adapter. No City/Knight engine offsets or shader identities are used.
// STATIC contracts: retail SHA in RE_NOTES; constructor ret 0x70, matrices words 14/15.
#include "scene.h"
#include "afw.h"
#include "dlss_bridge.h"
#include "reflect.h"
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
    const auto hc=gfx_hud::census();
    BVR_LOG("[bo] HUD: scene boundaries=%u, UI draws=%u, captured=%u, last-frame UI=%u",
        hc.boundaries,hc.hudDraws,hc.redirected,hc.lastFrameHud);
    reflect::status();
    transitions::status();
}
void __fastcall draw_detour(void* self,void* edx,W a1) {
    W ra=W(reinterpret_cast<uintptr_t>(_ReturnAddress())-imageBase);
    if(depth) { drawOriginal(self,edx,a1); return; }
    ++draws; note_caller(ra);
    command::poll_from_game_thread(GetTickCount64());
    if(GetAsyncKeyState(VK_F12)&1) recentre.store(true);
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
    vr::set_camera_mode(eligible);
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
    if(!foundView) vr::set_camera_mode(false); // startup movies / pure 2D roots use the room screen
    if(active && ordinary && foundView) {
        if(afwFrame) { afw::note_drawn(afwNext); afwNext^=1; }
        else {
            eyeSign=1;
            if(push_tag(self,1,false,poseGen) && second_draw(self,edx,a1)) ++seconds;
            else { poisoned.store(true); set_stereo(false); BVR_LOG("[bo] second render stopped; restart after fault/tag overflow"); }
        }
    }
    eyeSign=0; afwFrame=false; --depth; viewportNow=nullptr;
    static ULONGLONG previous=0;
    if(GetTickCount64()-previous>5000) { previous=GetTickCount64(); status(); }
}
void* __fastcall calc_detour(void* self,void* edx,void* family,float* loc,int32_t* rot,void* viewport,void* drawer) {
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
void settings_save() {
    wchar_t path[MAX_PATH]{}; swprintf_s(path,L"%s\\settings.ini",log::data_dir());
    wchar_t text[40]{};
    swprintf_s(text,L"%.2f",worldScale.load());
    WritePrivateProfileStringW(L"view",L"units_per_metre",text,path);
    wchar_t scale[40]{}; swprintf_s(scale,L"%.2f",resolutionScale.load());
    WritePrivateProfileStringW(L"launch",L"resolution_scale",scale,path);
    wchar_t mode[16]{}; swprintf_s(mode,L"%d",dlss_bridge::saved_mode());
    WritePrivateProfileStringW(L"launch",L"dlss",mode,path);
    float d,w,u; vr::get_hud_quad(&d,&w,&u);
    for(auto setting : {std::pair{L"distance",d},std::pair{L"width",w},std::pair{L"height",u}}) {
        swprintf_s(text,L"%.2f",setting.second);
        WritePrivateProfileStringW(L"hud",setting.first,text,path);
    }
}
}
namespace {
// MENUTEST (JJ 2026-10-11: in the database a skewed copy of the screen flickers on the left, the whole time).
// One button / command runs every candidate on a timer: 10 s each, a capture mid-step, a log line per step,
// then restores. JJ says afterwards in which step the copy was gone. Steps: 1 normal (control) 2 one HUD
// copy shown to both eyes (per-eye quad pair off) 3 AFW (one eye drawn per frame) 4 menu drawn straight
// into the eye pictures (no HUD layer).
std::atomic<bool> menuTestRunning{false};
DWORD WINAPI menu_test(void*) {
    const bool wasHud=hudOn.load(),wasNative=native.load(),wasAfw=afw::on(); const int wasEyes=vr::hud_eyes();
    const char* names[]={"1/4 normal (control)","2/4 one HUD copy for both eyes","3/4 one eye drawn per frame (AFW: the HUD drawn once per frame)",
        "4/4 menu drawn into the eye pictures (no HUD layer)"};
    for(int step=0;step<4;step++) {
        hudOn.store(step!=3);
        vr::set_hud_eyes(step==1 ? 0 : wasEyes);
        if(step==2 || step==3) afw::set_on(step==2 ? true : wasAfw);
        BVR_LOG("[bo] MENUTEST step %s",names[step]);
        if(step==0) {
            wchar_t dir[MAX_PATH]{}; swprintf_s(dir,L"%s\\captures\\menutest_sheets",log::data_dir());
            CreateDirectoryW(dir,nullptr); gfx_hud::request_panel_sequence(dir,8);
        }
        Sleep(5000); captureRequested.store(true); Sleep(5000);
    }
    hudOn.store(wasHud); vr::set_hud_eyes(wasEyes); afw::set_on(wasAfw); if(native.load()!=wasNative) set_stereo(wasNative);
    BVR_LOG("[bo] MENUTEST done - restored (hud %d, hud eyes %d, native %d)",wasHud,wasEyes,wasNative);
    menuTestRunning.store(false);
    return 0;
}
void start_menu_test() {
    if(menuTestRunning.exchange(true)) return;
    if(HANDLE t=CreateThread(nullptr,0,menu_test,nullptr,0,nullptr)) CloseHandle(t); else menuTestRunning.store(false);
}
}
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
    else if(!strcmp(args,"menutest")) start_menu_test();
    else if(!strncmp(args,"hudeyes ",8)) { vr::set_hud_eyes(atoi(args+8)); BVR_LOG("[bo] HUD per-eye copies %d",vr::hud_eyes()); }
    else if(!strcmp(args,"recenter")) recentre.store(true);
    else if(!strncmp(args,"afw ",4)) return afw::command(args+4);
    else if(!strncmp(args,"bats ",5)) return transitions::command(args+5);
    else if(!strncmp(args,"reflect ",8)) return reflect::command(args+8);
    else if(!strncmp(args,"dlss ",5)) { bool ok=dlss_bridge::command(args+5); settings_save(); return ok; }
    else return false;
    return true;
}
void draw_ui() {
    ImGui::TextUnformatted("BATMAN ORIGINS VR - development build");
    if(!installed.load()) { ImGui::TextWrapped("Engine guard failed. Headset screen remains available."); return; }
    if(poisoned.load()) ImGui::TextWrapped("Stereo stopped after an engine fault or label overflow. Restart required.");
    if(!gameplayCaller.load()) {
        ImGui::TextWrapped("Enter gameplay, then activate native stereo for the current viewport.");
        if(ImGui::Button("Activate native stereo in gameplay")) set_stereo(true);
    }
    if(ImGui::CollapsingHeader("STEREO RENDERING",ImGuiTreeNodeFlags_DefaultOpen)) {
        bool on=native.load() && gameplayCaller.load()!=0;
        if(ImGui::RadioButton("Native (both eyes drawn every frame)",on && !afw::on())) { afw::set_on(false); set_stereo(true); }
        if(ImGui::RadioButton("AFW (one eye drawn per frame, the other warped)",on && afw::on())) { afw::set_on(true); set_stereo(true); }
        if(afw::on()) afw::draw_ui();
    }
    if(ImGui::CollapsingHeader("VIEW")) {
        bool on=camera.load(); if(ImGui::Checkbox("Head tracking",&on)) set_camera(on);
        float scale=worldScale.load(); if(overlay::SliderStep("World scale (units per metre)",&scale,40,160,"%.0f")) { worldScale.store(scale); settings_save(); }
        if(ImGui::Button("Recentre (F12)")) recentre.store(true);
        if(ImGui::Button("Show game on a screen")) set_camera(false);
        reflect::draw_ui();
    }
    if(ImGui::CollapsingHeader("HUD")) {
        bool on=hudOn.load(); if(ImGui::Checkbox("HUD at depth",&on)) hudOn.store(on);
        float d,w,u; vr::get_hud_quad(&d,&w,&u); bool changed=false;
        changed |= overlay::SliderStep("HUD distance (m)",&d,.5f,10);
        changed |= overlay::SliderStep("HUD width (m)",&w,.5f,8);
        changed |= overlay::SliderStep("HUD height (m)",&u,-1,1);
        if(changed) { vr::set_hud_quad(d,w,u); settings_save(); }
        if(ImGui::Button(menuTestRunning.load() ? "Menu flicker test running..." : "Menu flicker test (40 s, open the menu first)"))
            start_menu_test();
    }
    if(ImGui::CollapsingHeader("MENUS AND SCREENS")) { bool on=vr::screen_room(); if(ImGui::Checkbox("Room-fixed screen",&on)) vr::set_screen_room(on); }
    if(ImGui::CollapsingHeader("BAT TRANSITIONS")) transitions::draw_ui();
    if(ImGui::CollapsingHeader("VR RESOLUTION")) {
        unsigned w=0,h=0; vr::recommended_eye_size(&w,&h);
        ImGui::Text("Runtime %ux%u; actual scene %ux%u",w,h,backW.load(),backH.load());
        float s=resolutionScale.load(); if(overlay::SliderStep("Resolution scale",&s,.6f,1.8f)) { resolutionScale.store(s); settings_save(); }
        ImGui::TextUnformatted("Resolution changes apply at next VR launch.");
    }
    if(ImGui::CollapsingHeader("DLSS (SMOOTHER EDGES)")) {
        const int before=dlss_bridge::saved_mode(); dlss_bridge::draw_ui();
        if(before!=dlss_bridge::saved_mode()) settings_save();
    }
    if(ImGui::Button("Capture both eyes (F2)")) captureRequested.store(true);
    ImGui::Text("Views changed %u; second renders %u; caller %08X",edits.load(),seconds.load(),gameplayCaller.load());
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
