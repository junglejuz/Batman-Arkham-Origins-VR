#include "transitions.h"
#include "wipe.h"
#include "core/gfx/gfx_hud.h"
#include "core/gfx/frame_inspector.h"
#include "core/gfx/blit.h"
#include "core/util/log.h"
#include "core/ui/overlay.h"
#include "core/vr/openxr_runtime.h"
#include <windows.h>
#include <d3d11.h>
#include <MinHook.h>
#include <imgui.h>
#include <algorithm>
#include <atomic>
#include <cstring>
#include <cstdio>
#include <vector>

namespace bvr::bo::transitions {
namespace {
// Origins' validated cdecl Wwise exports; no donor ABI or addresses.
using Post=DWORD(__cdecl*)(DWORD,DWORD,DWORD,void*,void*,DWORD,void*,DWORD);
Post original[3]{};
DWORD batEvent=0;
std::atomic<bool> enabled{true},recordNext{false};
std::atomic<float> distance{.8f},zoom{1.6f};
std::atomic<uint64_t> eventAt{0},originalUntil{0};
std::atomic<unsigned> events{0},draws{0},frames{0};
bool liveHooks=false,assetAttempted=false,assetReady=false,thisFrame=false;
unsigned checkW=0,checkH=0,outW=0,outH=0,appliedEvents=0,frameIndex=~0u;
ID3D11Device* owner=nullptr;
ID3D11Texture2D *source=nullptr,*mask=nullptr,*colourUi=nullptr,*colourSource=nullptr;
ID3D11ShaderResourceView *sourceSrv=nullptr,*colourSourceSrv=nullptr;
ID3D11RenderTargetView *maskRtv=nullptr,*colourUiRtv=nullptr;
unsigned colourW=0,colourH=0,autoCapturedEvent=0;
wipe::Recording recording;
wipe::Playback playback;
wipe::Step lastStep;
std::vector<uint8_t> pixels;
unsigned recordingFrame=0,recordedEvent=0;
wchar_t recordingDir[MAX_PATH]{};
template<class T> void drop(T*& p) {if(p) p->Release();p=nullptr;}
void release_targets() {
    drop(colourSourceSrv);drop(colourSource);drop(colourUiRtv);drop(colourUi);
    colourW=colourH=0;
    drop(sourceSrv);drop(maskRtv);drop(source);drop(mask);drop(owner);
    outW=outH=0;frameIndex=~0u;thisFrame=false;
}
bool load_recording() {
    if(assetAttempted) return assetReady;
    assetAttempted=true;
    HMODULE module=nullptr;
    if(!GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS|GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
        reinterpret_cast<LPCWSTR>(load_recording),&module)) return false;
    wchar_t path[MAX_PATH]{};
    if(!GetModuleFileNameW(module,path,MAX_PATH)) return false;
    auto* slash=wcsrchr(path,L'\\');if(!slash) return false;*slash=0;
    wcscat_s(path,L"\\OriginsVR\\bat-wipe.bin");
    FILE* f=nullptr;_wfopen_s(&f,path,L"rb");
    if(!f) {BVR_LOG("[bo bats] animation asset unavailable; original transitions retained");return false;}
    fseek(f,0,SEEK_END);const long size=ftell(f);rewind(f);
    std::vector<uint8_t> bytes;
    if(size>0 && size<64*1024*1024) bytes.resize(size);
    const bool read=!bytes.empty() && fread(bytes.data(),1,bytes.size(),f)==bytes.size();fclose(f);
    assetReady=read && wipe::decode(std::move(bytes),recording);
    if(assetReady) BVR_LOG("[bo bats] local City recording loaded: %ux%u, %u frames, %u ms",
        recording.width,recording.height,unsigned(recording.times.size()),recording.times.back());
    else BVR_LOG("[bo bats] invalid animation asset; original transitions retained");
    return assetReady;
}
bool permitted(uint64_t now) {return enabled.load() && now>=originalUntil.load();}
void synchronize(uint64_t now) {
    const unsigned n=events.load();
    if(!permitted(now)) {appliedEvents=n;playback.phase=wipe::Phase::Idle;return;}
    if(n!=appliedEvents) {
        appliedEvents=n;playback.event(eventAt.load());
        BVR_LOG("[bo bats] animation event %u: %s",n,playback.phase==wipe::Phase::Reveal?"black -> menu reveal":"bats -> black");
    }
}
void signal(uint64_t now) {
    // Name and numeric overloads may nest; one transition event, not three.
    const auto previous=eventAt.exchange(now);
    if(!previous || now-previous>100) {
        const auto n=events.fetch_add(1)+1;
        BVR_LOG("[bo bats] exact Play_UI_Fader_Bats event #%u",n);
    }
}
bool name_matches(DWORD ptr,bool wide) {
    static constexpr char expected[]="Play_UI_Fader_Bats";
    __try {
        for(unsigned i=0;i<sizeof(expected);++i) {
            unsigned c=wide ? reinterpret_cast<const wchar_t*>(ptr)[i] :
                             static_cast<unsigned char>(reinterpret_cast<const char*>(ptr)[i]);
            if(c>='A' && c<='Z') c+='a'-'A';
            unsigned e=static_cast<unsigned char>(expected[i]); if(e>='A' && e<='Z') e+='a'-'A';
            if(c!=e) return false;
        }
        return true;
    } __except(EXCEPTION_EXECUTE_HANDLER) { return false; }
}
DWORD __cdecl post_id(DWORD id,DWORD obj,DWORD flags,void* cb,void* cookie,DWORD n,void* ext,DWORD playing) {
    if(id==batEvent) signal(GetTickCount64());
    return original[0](id,obj,flags,cb,cookie,n,ext,playing);
}
DWORD __cdecl post_a(DWORD name,DWORD obj,DWORD flags,void* cb,void* cookie,DWORD n,void* ext,DWORD playing) {
    if(name_matches(name,false)) signal(GetTickCount64());
    return original[1](name,obj,flags,cb,cookie,n,ext,playing);
}
DWORD __cdecl post_w(DWORD name,DWORD obj,DWORD flags,void* cb,void* cookie,DWORD n,void* ext,DWORD playing) {
    if(name_matches(name,true)) signal(GetTickCount64());
    return original[2](name,obj,flags,cb,cookie,n,ext,playing);
}

bool ensure_targets(ID3D11DeviceContext* ctx) {
    if(!ctx || !load_recording()) return false;
    unsigned w=checkW,h=checkH;
    if(!w && !vr::output_eye_size(&w,&h) && !vr::recommended_eye_size(&w,&h)) return false;
    if(w<32 || h<32 || w>8192 || h>8192) return false;
    ID3D11Device* dev=nullptr;ctx->GetDevice(&dev);
    if(mask && owner==dev && outW==w && outH==h) {dev->Release();return true;}
    release_targets();owner=dev;
    D3D11_TEXTURE2D_DESC td{};td.Width=recording.width;td.Height=recording.height;
    td.MipLevels=td.ArraySize=td.SampleDesc.Count=1;
    td.Format=DXGI_FORMAT_R8_UNORM;td.BindFlags=D3D11_BIND_SHADER_RESOURCE;
    bool ok=SUCCEEDED(dev->CreateTexture2D(&td,nullptr,&source)) &&
        SUCCEEDED(dev->CreateShaderResourceView(source,nullptr,&sourceSrv));
    td.Width=w;td.Height=h;td.Format=DXGI_FORMAT_R8G8B8A8_UNORM;
    td.BindFlags=D3D11_BIND_RENDER_TARGET|D3D11_BIND_SHADER_RESOURCE;
    ok=ok && SUCCEEDED(dev->CreateTexture2D(&td,nullptr,&mask)) &&
        SUCCEEDED(dev->CreateRenderTargetView(mask,nullptr,&maskRtv));
    if(ok) {
        frame_inspector::ScopedSuppress quiet;
        if(recording.decode_frame(0,pixels)) {
            ctx->UpdateSubresource(source,0,nullptr,pixels.data(),recording.width,0);frameIndex=0;
            const float clear[4]{};ctx->ClearRenderTargetView(maskRtv,clear);
            ok=blit::mask_black(ctx,maskRtv,w,h,sourceSrv,1.f);
        } else ok=false;
    }
    if(!ok) {release_targets();BVR_LOG("[bo bats] replay resource/shader failed; original UI retained");return false;}
    outW=w;outH=h;
    BVR_LOG("[bo bats] animated coverage %ux%u -> %ux%u output, independent of DLSS",recording.width,recording.height,w,h);
    return true;
}
bool owns(ID3D11DeviceContext* ctx) {
    const auto now=GetTickCount64();synchronize(now);
    return permitted(now) && playback.owns() && ensure_targets(ctx);
}
bool draw(ID3D11DeviceContext* ctx,unsigned,bool,gfx_hud::Decision& d) {
    // Observe the proved UI lane, but NEVER discard the original bat/menu
    // drawing. Filter its black coverage only after a replacement was copied.
    if(owns(ctx)) draws.fetch_add(1);
    return false;
}
void prepare(ID3D11DeviceContext* ctx,IDXGISwapChain*) {
    owns(ctx); // prepare resources, preserving the complete original UI sheet
}
ID3D11Texture2D* filter_ui(ID3D11DeviceContext* ctx,ID3D11Texture2D* ui) {
    if(!ui || !owner) return ui;
    D3D11_TEXTURE2D_DESC td{};ui->GetDesc(&td);
    if(!colourUi || colourW!=td.Width || colourH!=td.Height) {
        drop(colourUiRtv);drop(colourUi);drop(colourSourceSrv);drop(colourSource);
        td.Usage=D3D11_USAGE_DEFAULT;td.BindFlags=D3D11_BIND_RENDER_TARGET|D3D11_BIND_SHADER_RESOURCE;
        td.CPUAccessFlags=td.MiscFlags=0;
        if(FAILED(owner->CreateTexture2D(&td,nullptr,&colourUi)) ||
            FAILED(owner->CreateRenderTargetView(colourUi,nullptr,&colourUiRtv))) return ui;
        colourW=td.Width;colourH=td.Height;
    }
    if(colourSource!=ui) {
        drop(colourSourceSrv);drop(colourSource);
        if(FAILED(owner->CreateShaderResourceView(ui,nullptr,&colourSourceSrv))) return ui;
        colourSource=ui;ui->AddRef();
    }
    frame_inspector::ScopedSuppress quiet;
    const float clear[4]{};ctx->ClearRenderTargetView(colourUiRtv,clear);
    return blit::transition_part(ctx,colourUiRtv,colourW,colourH,colourSourceSrv,1,false) ? colourUi:ui;
}
ID3D11Texture2D* hud_texture(ID3D11DeviceContext* ctx) {
    auto* ui=gfx_hud::texture(ctx);
    // Failed XR setup/copy keeps the untouched original sheet and its bats.
    return thisFrame && vr::transition_layer_ready() ? filter_ui(ctx,ui):ui;
}
float smooth(float x) {x=std::clamp(x,0.f,1.f);return x*x*(3-2*x);}
bool render_step(ID3D11DeviceContext* ctx,const wipe::Step& step) {
    frame_inspector::ScopedSuppress quiet;
    const float clear[4]={0,0,0,step.visual==wipe::Visual::Black ? 1.f:0.f};
    ctx->ClearRenderTargetView(maskRtv,clear);
    if(step.visual!=wipe::Visual::Animation) return true;
    const auto it=std::upper_bound(recording.times.begin(),recording.times.end(),step.elapsed);
    const unsigned index=it==recording.times.begin()?0:unsigned(it-recording.times.begin()-1);
    if(index!=frameIndex) {
        if(!recording.decode_frame(index,pixels)) return false;
        ctx->UpdateSubresource(source,0,nullptr,pixels.data(),recording.width,0);frameIndex=index;
    }
    // Finish at an exact clear/opaque endpoint without normalizing away bats
    // or shrinking the recorded image's animated gaps into a box.
    const float tail=smooth(float(int64_t(step.elapsed)-int64_t(recording.times.back()-120))/120.f);
    return blit::mask_black(ctx,maskRtv,outW,outH,sourceSrv,zoom.load(),step.invert,
        step.invert ? 1-tail:1.f,step.invert ? 0.f:tail);
}
ID3D11Texture2D* texture(ID3D11DeviceContext* ctx,bool projection) {
    const auto now=GetTickCount64();synchronize(now);thisFrame=false;
    if(!permitted(now) || !ensure_targets(ctx)) return nullptr;
    lastStep=playback.tick(now,projection,recording.times.back());
    if(lastStep.visual==wipe::Visual::None) return nullptr;
    if(!render_step(ctx,lastStep)) {playback.phase=wipe::Phase::Idle;return nullptr;}
    thisFrame=true;frames.fetch_add(1);return mask;
}
void end(ID3D11DeviceContext* ctx) {
    // One paired UI/mask evidence sample per first two real events. This
    // captures the content behind the matte, which O1F's tests omitted.
    if(liveHooks && appliedEvents && appliedEvents<=2 && appliedEvents!=autoCapturedEvent &&
        thisFrame && lastStep.visual==wipe::Visual::Animation && lastStep.elapsed>=400) {
        autoCapturedEvent=appliedEvents;
        wchar_t dir[MAX_PATH]{};swprintf_s(dir,L"%s\\bat_layers_%llu_event_%u",log::data_dir(),GetTickCount64(),appliedEvents);
        CreateDirectoryW(dir,nullptr);save_capture(ctx,dir);
        BVR_LOG("[bo bats] paired raw UI / preserved UI / coverage saved in %ls",dir);
    }
    if(thisFrame && appliedEvents!=recordedEvent && recordNext.exchange(false)) {
        recordedEvent=appliedEvents;
        swprintf_s(recordingDir,L"%s\\bat_transition_%llu",log::data_dir(),GetTickCount64());
        CreateDirectoryW(recordingDir,nullptr);recordingFrame=1;
    }
    if(recordingFrame && thisFrame) {
        if(recordingFrame==1 || recordingFrame==8 || recordingFrame==20) {
            wchar_t dir[MAX_PATH]{};swprintf_s(dir,L"%s\\frame_%03u",recordingDir,recordingFrame);
            CreateDirectoryW(dir,nullptr);save_capture(ctx,dir);vr::save_eye_pictures(dir);
        }
        if(++recordingFrame>20) {recordingFrame=0;BVR_LOG("[bo bats] transition captures saved in %ls",recordingDir);}
    } else if(recordingFrame) recordingFrame=0;
    thisFrame=false;
}
void settings_save() {
    wchar_t path[MAX_PATH]{},v[48]{};swprintf_s(path,L"%s\\settings.ini",log::data_dir());
    WritePrivateProfileStringW(L"bats",L"enabled",enabled.load()?L"1":L"0",path);
    swprintf_s(v,L"%.2f",distance.load());WritePrivateProfileStringW(L"bats",L"distance",v,path);
    swprintf_s(v,L"%.2f",zoom.load());WritePrivateProfileStringW(L"bats",L"enlargement",v,path);
}
}
void enable_for_check(unsigned w,unsigned h) {
    enabled.store(true);originalUntil.store(0);checkW=w;checkH=h;
    gfx_hud::set_draw_override(draw,nullptr,end,nullptr);gfx_hud::set_before_capture_hook(prepare);
}
bool install() {
    HMODULE exe=GetModuleHandleW(nullptr);
    constexpr const char* names[]={
        "?PostEvent@SoundEngine@AK@@YAKKIKP6AXW4AkCallbackType@@PAUAkCallbackInfo@@@ZPAXKPAUAkExternalSourceInfo@@K@Z",
        "?PostEvent@SoundEngine@AK@@YAKPBDIKP6AXW4AkCallbackType@@PAUAkCallbackInfo@@@ZPAXKPAUAkExternalSourceInfo@@K@Z",
        "?PostEvent@SoundEngine@AK@@YAKPB_WIKP6AXW4AkCallbackType@@PAUAkCallbackInfo@@@ZPAXKPAUAkExternalSourceInfo@@K@Z"};
    const unsigned rvas[]={0xd7b2b0,0xd7a8f0,0xd7a860};
    const unsigned char prefix[]={0x55,0x8b,0xec,0x83,0xec,0x10,0x53,0x56,0x57,0x8b,0x7d,0x1c,0x33,0xf6};
    void* targets[3]{};void* hooks[]={reinterpret_cast<void*>(post_id),reinterpret_cast<void*>(post_a),reinterpret_cast<void*>(post_w)};
    using Hash=DWORD(__cdecl*)(const char*);
    auto hash=reinterpret_cast<Hash>(GetProcAddress(exe,"?GetIDFromString@SoundEngine@AK@@YAKPBD@Z"));
    if(!hash || uintptr_t(hash)!=uintptr_t(exe)+0xd78560) return false;
    for(unsigned i=0;i<3;++i) {
        targets[i]=reinterpret_cast<void*>(GetProcAddress(exe,names[i]));
        if(uintptr_t(targets[i])!=uintptr_t(exe)+rvas[i] || memcmp(targets[i],prefix,sizeof(prefix))) {
            BVR_LOG("[bo bats] Origins event export guard failed; original transition retained");return false;
        }
    }
    batEvent=hash("Play_UI_Fader_Bats");
    if(batEvent!=0xe8bb4d2c) {
        BVR_LOG("[bo bats] Origins event hash guard failed; original transition retained");return false;
    }
    unsigned made=0;
    for(;made<3;++made) {
        if(MH_CreateHook(targets[made],hooks[made],reinterpret_cast<void**>(&original[made]))!=MH_OK) break;
        if(MH_QueueEnableHook(targets[made])!=MH_OK) { MH_RemoveHook(targets[made]);break; }
    }
    if(made!=3 || MH_ApplyQueued()!=MH_OK) {
        for(unsigned i=0;i<made;++i) MH_QueueDisableHook(targets[i]);MH_ApplyQueued();
        for(unsigned i=0;i<made;++i) MH_RemoveHook(targets[i]);return false;
    }
    liveHooks=true;
    wchar_t path[MAX_PATH]{},v[48]{};swprintf_s(path,L"%s\\settings.ini",log::data_dir());
    enabled.store(GetPrivateProfileIntW(L"bats",L"enabled",1,path)!=0);
    GetPrivateProfileStringW(L"bats",L"distance",L"0.8",v,48,path);
    distance.store(std::clamp(float(wcstod(v,nullptr)),.4f,2.f));
    GetPrivateProfileStringW(L"bats",L"enlargement",L"1.6",v,48,path);
    zoom.store(std::clamp(float(wcstod(v,nullptr)),1.f,2.5f));
    gfx_hud::set_draw_override(draw,nullptr,end,nullptr);
    gfx_hud::set_before_capture_hook(prepare);
    vr::set_transition_texture_provider(texture);vr::set_transition_distance(distance.load());
    vr::set_hud_texture_provider(hud_texture);
    BVR_LOG("[bo bats] Origins event hooks ready, id %u; full-view recorded bat animation, %.2f m, %.2fx",batEvent,distance.load(),zoom.load());
    return true;
}

void save_capture(ID3D11DeviceContext* ctx,const wchar_t* dir) {
    wchar_t path[MAX_PATH]{};
    if(auto* hud=gfx_hud::texture(ctx)) {swprintf_s(path,L"%s\\hud_panel.bmp",dir);gfx_hud::save_texture_bmp(ctx,hud,path);}
    if(thisFrame && colourUi) {swprintf_s(path,L"%s\\preserved_menu.bmp",dir);gfx_hud::save_texture_bmp(ctx,colourUi,path);}
    if(thisFrame && mask) {swprintf_s(path,L"%s\\transition_coverage.bmp",dir);gfx_hud::save_texture_bmp(ctx,mask,path);}
    swprintf_s(path,L"%s\\hud_last_frames.txt",dir);gfx_hud::save_hud_ring(path);
    swprintf_s(path,L"%s\\transition.txt",dir);FILE* f=nullptr;_wfopen_s(&f,path,L"w");
    if(f) {fprintf(f,"event=%u events=%u phase=%u visual=%u elapsed=%llu frame=%u invert=%d\nsource=%ux%u output=%ux%u distance=%.2f enlargement=%.2f\nSeparate head-relative XR coverage quads; eye BMPs exclude those layers.\n",
        batEvent,events.load(),unsigned(playback.phase),unsigned(lastStep.visual),lastStep.elapsed,frameIndex,lastStep.invert,
        recording.width,recording.height,outW,outH,distance.load(),zoom.load());fclose(f);}
    const auto submit=vr::transition_status();
    swprintf_s(path,L"%s\\submission.txt",dir);_wfopen_s(&f,path,L"w");
    if(f) {fprintf(f,"generated=%u copied=%u submitted=%u failures=%u ready=%d\nThese count actual XR copy/layer calls; headset visibility is a separate verdict.\n",
        submit.generated,submit.copied,submit.submitted,submit.failures,vr::transition_layer_ready());fclose(f);}
}
void status() {
    BVR_LOG("[bo bats] hooks=%d on=%d event=%u signals=%u preserved-draws=%u frames=%u asset=%d phase=%u output=%ux%u",
        liveHooks,enabled.load(),batEvent,events.load(),draws.load(),frames.load(),assetReady,unsigned(playback.phase),outW,outH);
    const auto submit=vr::transition_status();
    BVR_LOG("[bo bats] XR generated=%u copied=%u submitted=%u failed=%u",submit.generated,submit.copied,submit.submitted,submit.failures);
}
bool command(const char* args) {
    if(!strcmp(args,"on")) enabled.store(true);
    else if(!strcmp(args,"off")) enabled.store(false);
    else if(!strcmp(args,"original")) originalUntil.store(GetTickCount64()+15000);
    else if(!strcmp(args,"record")) recordNext.store(true);
    else if(!strcmp(args,"status")) { status();return true; }
    else return false;
    settings_save();return true;
}
void draw_ui() {
    bool on=enabled.load();if(ImGui::Checkbox("Full-view bat transitions",&on)) {enabled.store(on);settings_save();}
    float d=distance.load(),z=zoom.load();bool changed=false;
    changed |= overlay::SliderStep("Bat distance (m)",&d,.4f,2.f);
    changed |= overlay::SliderStep("Bat enlargement",&z,1.f,2.5f);
    if(changed) {distance.store(d);zoom.store(z);vr::set_transition_distance(d);settings_save();}
    if(ImGui::Button("Original transitions for 15 seconds")) originalUntil.store(GetTickCount64()+15000);
    const auto now=GetTickCount64(),until=originalUntil.load();
    if(now<until) ImGui::Text("Original view: %u seconds remaining",unsigned((until-now+999)/1000));
    if(ImGui::Button("Record next bat transition")) recordNext.store(true);
    if(recordNext.load()) ImGui::TextUnformatted("Waiting for the next bat transition.");
    ImGui::Text("Events %u; transition draws %u; output %ux%u",events.load(),draws.load(),outW,outH);
    if(!liveHooks) ImGui::TextUnformatted("Transition event hooks unavailable; original view retained.");
}

namespace {
DWORD forwarded[8]{};
DWORD __cdecl test_post(DWORD a,DWORD b,DWORD c,void* d,void* e,DWORD f,void* g,DWORD h) {
    DWORD args[]={a,b,c,DWORD(d),DWORD(e),f,DWORD(g),h};
    memcpy(forwarded,args,sizeof(args));return 0x12345678;
}
bool pixel_at(ID3D11DeviceContext* ctx,ID3D11Texture2D* src,unsigned char out[4],unsigned x,unsigned y) {
    D3D11_TEXTURE2D_DESC td{};src->GetDesc(&td);
    td.Usage=D3D11_USAGE_STAGING;td.BindFlags=td.MiscFlags=0;td.CPUAccessFlags=D3D11_CPU_ACCESS_READ;
    ID3D11Texture2D* staging=nullptr;ID3D11Device* dev=nullptr;ctx->GetDevice(&dev);
    bool ok=SUCCEEDED(dev->CreateTexture2D(&td,nullptr,&staging));dev->Release();
    if(ok) {
        ctx->CopyResource(staging,src);D3D11_MAPPED_SUBRESOURCE mp{};
        ok=SUCCEEDED(ctx->Map(staging,0,D3D11_MAP_READ,0,&mp));
        if(ok) {memcpy(out,static_cast<const char*>(mp.pData)+y*mp.RowPitch+x*4,4);ctx->Unmap(staging,0);}
    }
    drop(staging);return ok;
}

}
extern "C" DWORD WINAPI BO_Identity();
extern "C" __declspec(dllexport) DWORD WINAPI BO_TransitionSelftest(ID3D11DeviceContext* ctx,IDXGISwapChain* sc) {
    if(BO_Identity()!=2 || liveHooks || !ctx || !sc) return 1;
    bool ok=true;
    auto check=[&](bool pass,const char* label) {if(!pass) fprintf(stderr,"Transition check failed: %s\n",label);ok &= pass;};
    check(name_matches(DWORD("PLAY_UI_FADER_BATS"),false) && name_matches(DWORD(L"Play_UI_Fader_Bats"),true) &&
        !name_matches(DWORD("Play_UI_Fader_BatsExtra"),false) && !name_matches(DWORD("Play_UI_Back"),false),"exact event names");
    const DWORD previousId=batEvent;batEvent=0xe8bb4d2c;
    for(auto& fn:original) fn=test_post;
    for(int n=0;n<1024;++n) {
        DWORD first[]={batEvent,DWORD("Play_UI_Fader_Bats"),DWORD(L"Play_UI_Fader_Bats")};Post funcs[]={post_id,post_a,post_w};
        for(unsigned i=0;i<3;++i) {
            const DWORD expected[]={first[i],2,3,4,5,6,7,8};
            check(funcs[i](first[i],2,3,reinterpret_cast<void*>(4),reinterpret_cast<void*>(5),6,reinterpret_cast<void*>(7),8)==0x12345678 &&
                !memcmp(forwarded,expected,sizeof(expected)),"eight-word audio forwarding");
        }
    }
    for(auto& fn:original) fn=nullptr;batEvent=previousId;
    check(load_recording() && recording.width==1440 && recording.height==1536 && recording.times.size()==70 &&
        recording.times.back()==1172,"actual private recording loaded");
    if(!assetReady) return 2;
    wipe::Recording bad;
    auto corrupt=recording.bytes;corrupt.pop_back();check(!wipe::decode(corrupt,bad),"truncated RLE rejected");
    corrupt=recording.bytes;corrupt[recording.offsets[0]]=0;check(!wipe::decode(corrupt,bad),"zero run rejected");
    corrupt=recording.bytes;corrupt[24]=0;corrupt[25]=0;corrupt[26]=0;corrupt[27]=0;
    check(!wipe::decode(corrupt,bad),"nonincreasing times rejected");
    corrupt=recording.bytes;corrupt.push_back(0);check(!wipe::decode(corrupt,bad),"trailing data rejected");
    wipe::Playback flow;
    check(flow.tick(1000,false,1172).visual==wipe::Visual::None && flow.tick(1000,true,1172).visual==wipe::Visual::None &&
        !flow.owns(),"logos and 3D title prompt remain visible before exit");
    flow.event(1000);
    check(flow.tick(1000,true,1172).visual==wipe::Visual::Black && flow.tick(1500,true,1172).invert,"first event reveals from black");
    check(flow.tick(2172,true,1172).visual==wipe::Visual::None && !flow.owns(),"first menu restored");
    wipe::Playback delayed;delayed.event(1000);
    check(delayed.tick(1250,true,1172).visual==wipe::Visual::Black && delayed.start==1250 &&
        delayed.tick(1270,true,1172).invert,"first presented exit frame black even when rendering is delayed");
    for(unsigned route=0;route<3;++route) {
        const uint64_t at=3000+route*10000;flow.event(at);
        flow.tick(at,true,1172);
        check(!flow.tick(at+500,true,1172).invert,"later transition bats cover");
        flow.tick(at+1100,false,1172);flow.tick(at+1200,true,1172);
        check(flow.tick(at+1500,true,1172).visual==wipe::Visual::Black,"transient projection not loading completion");
        flow.tick(at+1510,false,1172);flow.tick(at+4400,true,1172);
        check(flow.tick(at+4749,true,1172).visual==wipe::Visual::Black,"loading settle black");
        flow.tick(at+4750,true,1172);
        check(flow.tick(at+5000,true,1172).invert,"arriving scene revealed by bats");
        check(flow.tick(at+5922,true,1172).visual==wipe::Visual::None,"ordinary UI returns");
    }
    flow.event(40000);flow.tick(40000,true,1172);flow.tick(41200,false,1172);
    check(flow.tick(101200,false,1172).visual==wipe::Visual::None,"loading watchdog returns original");
    enable_for_check(96,104);zoom.store(1.f);
    check(ensure_targets(ctx),"D3D replay resources");
    if(mask) {
        unsigned char c[4]{};
        render_step(ctx,{wipe::Visual::Black});
        for(auto xy : {std::pair<unsigned,unsigned>{0,0},{95,0},{0,103},{95,103},{48,52}})
            check(pixel_at(ctx,mask,c,xy.first,xy.second) && c[0]==0 && c[1]==0 && c[2]==0 && c[3]==255,"opaque black at all corners and centre");
        ID3D11Device* dev=nullptr;ctx->GetDevice(&dev);ID3D11Buffer* cb=nullptr;
        D3D11_BUFFER_DESC bd{};bd.ByteWidth=16;bd.BindFlags=D3D11_BIND_CONSTANT_BUFFER;
        check(SUCCEEDED(dev->CreateBuffer(&bd,nullptr,&cb)),"state sentinel");
        ctx->VSSetConstantBuffers(4,1,&cb);ctx->PSSetConstantBuffers(4,1,&cb);
        std::vector<uint8_t> cover,reveal;
        check(recording.decode_frame(30,cover),"bat frame decode");
        unsigned changed=0,gaps=0,opaque=0;
        for(auto v:cover) {gaps+=v<20;opaque+=v>235;}
        check(gaps>1000 && opaque>1000,"actual animation has shapes and gaps");
        check(recording.decode_frame(10,reveal),"earlier bat frame decode");
        for(unsigned i=0;i<cover.size();++i) changed+=cover[i]!=reveal[i];
        check(changed>10000,"recorded bats move across frames");
        check(render_step(ctx,{wipe::Visual::Animation,false,recording.times[30]}),"cover GPU shader");
        std::vector<unsigned char> rendered(96*104);
        for(unsigned y=0;y<104;y+=13) for(unsigned x=0;x<96;x+=12) {
            check(pixel_at(ctx,mask,c,x,y),"coverage readback");rendered[y*96+x]=c[3];
        }
        check(render_step(ctx,{wipe::Visual::Animation,true,recording.times[30]}),"reveal GPU shader");
        unsigned sampleGaps=0,sampleBats=0;
        for(unsigned y=0;y<104;y+=13) for(unsigned x=0;x<96;x+=12) {
            check(pixel_at(ctx,mask,c,x,y) && abs(int(c[3])+int(rendered[y*96+x])-255)<=2,"reveal inverts cover rather than crop");
            sampleGaps+=rendered[y*96+x]<20;sampleBats+=rendered[y*96+x]>235;
        }
        check(sampleGaps && sampleBats,"GPU mask keeps visible bat gaps");
        check(render_step(ctx,{wipe::Visual::Animation,false,1172}) && pixel_at(ctx,mask,c,0,0) && c[3]==255,"cover ends fully black including source border");
        check(render_step(ctx,{wipe::Visual::Animation,true,1172}) && pixel_at(ctx,mask,c,0,0) && c[3]==0,"reveal ends fully clear");
        // Regression fixture: a black transition stage and actual menu colour
        // underneath an inverted recording. O1F's blanket skip made this whole
        // composite black; an isolated alpha-mask test could not detect that.
        ID3D11Texture2D *ui=nullptr,*final=nullptr,*xrCopy=nullptr;
        ID3D11RenderTargetView *uiRtv=nullptr,*finalRtv=nullptr;
        ID3D11ShaderResourceView *preservedSrv=nullptr,*maskSrv=nullptr;
        D3D11_TEXTURE2D_DESC td{};td.Width=96;td.Height=104;td.MipLevels=td.ArraySize=td.SampleDesc.Count=1;
        td.Format=DXGI_FORMAT_R8G8B8A8_UNORM;td.BindFlags=D3D11_BIND_RENDER_TARGET|D3D11_BIND_SHADER_RESOURCE;
        bool fixture=SUCCEEDED(dev->CreateTexture2D(&td,nullptr,&ui)) && SUCCEEDED(dev->CreateRenderTargetView(ui,nullptr,&uiRtv)) &&
            SUCCEEDED(dev->CreateTexture2D(&td,nullptr,&final)) && SUCCEEDED(dev->CreateRenderTargetView(final,nullptr,&finalRtv));
        td.Format=DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;
        fixture=fixture && SUCCEEDED(dev->CreateTexture2D(&td,nullptr,&xrCopy));
        if(fixture) {
            const float stage[4]={.01f,.01f,.01f,1};ctx->ClearRenderTargetView(uiRtv,stage);
            std::vector<unsigned char> menu(80*88*4);
            for(unsigned i=0;i<menu.size();i+=4) {menu[i]=51;menu[i+1]=128;menu[i+2]=204;menu[i+3]=255;}
            D3D11_BOX box{8,8,0,88,96,1};ctx->UpdateSubresource(ui,0,&box,menu.data(),80*4,0);
            auto* kept=filter_ui(ctx,ui);
            check(kept && kept!=ui && pixel_at(ctx,kept,c,0,0) && c[3]==0,"old black UI stage removed without discarding draws");
            check(kept && pixel_at(ctx,kept,c,48,52) && c[0]==51 && c[1]==128 && c[2]==204 && c[3]==255,"menu content retained through transition");
            fixture=kept && SUCCEEDED(dev->CreateShaderResourceView(kept,nullptr,&preservedSrv)) &&
                SUCCEEDED(dev->CreateShaderResourceView(mask,nullptr,&maskSrv));
            if(fixture) {
                const float black[4]={0,0,0,1};ctx->ClearRenderTargetView(finalRtv,black);
                check(blit::alpha_premul(ctx,finalRtv,preservedSrv,96,104),"preserved menu composite");
                check(render_step(ctx,{wipe::Visual::Animation,true,recording.times[30]}) &&
                    blit::alpha_premul(ctx,finalRtv,maskSrv,96,104),"actual bat reveal over preserved menu");
                unsigned visibleMenu=0,hiddenMenu=0;
                for(unsigned y=12;y<96;y+=12) for(unsigned x=12;x<88;x+=12) {
                    check(pixel_at(ctx,final,c,x,y),"final composite readback");
                    visibleMenu+=c[2]>180;hiddenMenu+=c[2]<10;
                }
                check(visibleMenu && hiddenMenu,"final image has menu revealed through bat shapes, not black on black");
                check(blit::copy_rgba_layer(ctx,xrCopy,mask,DXGI_FORMAT_R8G8B8A8_UNORM_SRGB),"actual typed XR transfer path");
                unsigned char copied[4]{};
                check(pixel_at(ctx,mask,c,48,52) && pixel_at(ctx,xrCopy,copied,48,52) &&
                    !memcmp(c,copied,4),"UNORM-to-sRGB XR-family copy preserves bat coverage alpha");
                // Runtime resources are often TYPELESS, regardless of the
                // requested typed format. Exercise the same helper as XR,
                // including its explicit-view BGRA conversion fallback.
                for(auto fmt : {DXGI_FORMAT_R8G8B8A8_TYPELESS,DXGI_FORMAT_B8G8R8A8_TYPELESS}) {
                    drop(xrCopy);td.Format=fmt;
                    check(SUCCEEDED(dev->CreateTexture2D(&td,nullptr,&xrCopy)),"typeless runtime-shaped image");
                    if(!xrCopy) continue;
                    ID3D11RenderTargetView* invalid=nullptr;
                    check(FAILED(dev->CreateRenderTargetView(xrCopy,nullptr,&invalid)),"reproduces O1G null-view failure on typeless images");drop(invalid);
                    const auto view=fmt==DXGI_FORMAT_R8G8B8A8_TYPELESS ? DXGI_FORMAT_R8G8B8A8_UNORM_SRGB:DXGI_FORMAT_B8G8R8A8_UNORM_SRGB;
                    check(blit::copy_rgba_layer(ctx,xrCopy,mask,view),"actual typeless runtime transfer path");
                    for(unsigned y=0;y<104;y+=13) for(unsigned x=0;x<96;x+=12)
                        check(pixel_at(ctx,mask,c,x,y) && pixel_at(ctx,xrCopy,copied,x,y) &&
                            !memcmp(c,copied,4),"typeless transfer preserves bat shapes, gaps and coverage alpha");
                    check(render_step(ctx,{wipe::Visual::Black}) && blit::copy_rgba_layer(ctx,xrCopy,mask,view),"opening black reaches runtime-shaped image");
                    for(auto xy : {std::pair<unsigned,unsigned>{0,0},{95,0},{0,103},{95,103},{48,52}})
                        check(pixel_at(ctx,xrCopy,c,xy.first,xy.second) && c[0]==0 && c[1]==0 && c[2]==0 && c[3]==255,"full-field black survives runtime transfer");
                    check(render_step(ctx,{wipe::Visual::Animation,true,1172}) && blit::copy_rgba_layer(ctx,xrCopy,mask,view) &&
                        pixel_at(ctx,xrCopy,c,48,52) && c[3]==0,"reveal clears runtime coverage at end");
                    check(render_step(ctx,{wipe::Visual::Animation,true,recording.times[30]}),"restore animated mask for next format");
                }
                wchar_t dir[MAX_PATH]{};
                if(GetEnvironmentVariableW(L"BO_WARP_CAPTURE_DIR",dir,MAX_PATH)) {
                    CreateDirectoryW(dir,nullptr);wchar_t path[MAX_PATH]{};
                    swprintf_s(path,L"%s\\raw_ui.bmp",dir);gfx_hud::save_texture_bmp(ctx,ui,path);
                    swprintf_s(path,L"%s\\preserved_ui.bmp",dir);gfx_hud::save_texture_bmp(ctx,kept,path);
                    swprintf_s(path,L"%s\\visible_bat_reveal.bmp",dir);gfx_hud::save_texture_bmp(ctx,final,path);
                }
            }
        }
        check(fixture,"full bat/menu composite fixture setup");
        drop(preservedSrv);drop(maskSrv);drop(uiRtv);drop(finalRtv);drop(ui);drop(final);drop(xrCopy);
        ID3D11Buffer *v=nullptr,*s=nullptr;ctx->VSGetConstantBuffers(4,1,&v);ctx->PSGetConstantBuffers(4,1,&s);
        check(v==cb && s==cb,"VS/PS constant buffers restored");drop(v);drop(s);drop(cb);drop(dev);
        enabled.store(false);gfx_hud::Decision d{};check(!draw(ctx,30,true,d) && !d.skip,"disabled keeps original draw");
        enabled.store(true);originalUntil.store(GetTickCount64()+15000);
        check(!draw(ctx,30,true,d) && !texture(ctx,true),"original mode keeps UI and omits replacement");
        originalUntil.store(0);playback.phase=wipe::Phase::Cover;playback.start=GetTickCount64();
        check(!draw(ctx,30,true,d) && !d.skip && !d.redirectRtv && !d.after,"replay preserves original bat/menu draws");
        playback.phase=wipe::Phase::Idle;d={};check(!draw(ctx,30,true,d),"UI restored after replay");
    }
    gfx_hud::set_draw_override(nullptr,nullptr,nullptr,nullptr);gfx_hud::set_before_capture_hook(nullptr);
    ctx->ClearState();release_targets();checkW=checkH=0;eventAt.store(0);events.store(0);appliedEvents=0;
    playback={};pixels.clear();
    return ok?0:2;
}
}
