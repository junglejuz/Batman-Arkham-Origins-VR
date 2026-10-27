#include "transitions.h"
#include "wipe.h"
#include "core/gfx/gfx_hud.h"
#include "core/gfx/hud_capture.h"
#include "core/gfx/frame_inspector.h"
#include "core/gfx/blit.h"
#include "core/util/log.h"
#include "core/ui/overlay.h"
#include "core/vr/openxr_runtime.h"
#include <windows.h>
#include <d3d11.h>
#include <d3dcompiler.h>
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
// EYEBATS (JJ 2026-10-13): the bats and the black are painted into each finished eye image (full view, like City)
// instead of a separate quad layer, which Virtual Desktop showed as a window. eyeZoom = the recording's size against
// the eye's field of view (City's in-eye 1.6; the old quad "enlargement" is kept only for the self-test).
std::atomic<float> eyeZoom{1.6f};
std::atomic<uint64_t> lastEyeMs{0},lastProjectionMs{0};
std::atomic<uint32_t> eyePaints{0};
int64_t lastFrame=-1;uint64_t lastTickMs=0;
// BOOTBLACK (JJ 2026-10-13: "a black window that appears after the title screen video and before the first menu"):
// after the opening movies Origins covers its UI stage with plain untextured fills (~1.6 s, O1H log 23:47:34-35.6:
// HUD frames of 1-2 untextured draws, nothing else) = a full-screen black in 2D, a black window on the VR panel.
// Until the first menu is up, a UI frame of ONLY untextured draws makes the whole view black (City BLACKFILL).
std::atomic<bool> bootDone{false};
std::atomic<uint32_t> frameTextured{0},frameUntextured{0};
uint64_t bootBlackUntil=0;unsigned texturedRun=0,bootBlackLogs=0;
std::atomic<uint64_t> eventAt{0},originalUntil{0};
std::atomic<unsigned> events{0},draws{0},frames{0};
bool liveHooks=false,assetAttempted=false,assetReady=false,thisFrame=false;
unsigned checkW=0,checkH=0,outW=0,outH=0,appliedEvents=0,frameIndex=~0u;
ID3D11Device* owner=nullptr;
ID3D11Texture2D *source=nullptr,*mask=nullptr,*colourUi=nullptr,*colourSource=nullptr;
ID3D11ShaderResourceView *sourceSrv=nullptr,*colourSourceSrv=nullptr;
ID3D11RenderTargetView *maskRtv=nullptr,*colourUiRtv=nullptr;
unsigned colourW=0,colourH=0;
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
std::atomic<bool> probeFastTravel{false}; // FTPROBE: snapshot the next fast travel (command "probe")
void synchronize(uint64_t now) {
    const unsigned n=events.load();
    if(!permitted(now)) {appliedEvents=n;playback.phase=wipe::Phase::Idle;return;}
    if(n!=appliedEvents) {
        appliedEvents=n;playback.event(eventAt.load());
        BVR_LOG("[bo bats] animation event %u: %s",n,playback.phase==wipe::Phase::Reveal?"black -> menu reveal":"bats -> black");
        // FTPROBE (JJ 2026-10-26): on request only ("bo bats probe" in command.txt) since the video showed in run 23:40.
        if(gfx_hud::menu_screen_on() && probeFastTravel.exchange(false)) vr::probe_arm("bats while the map is open = fast travel");
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

// The recording's one-channel frame texture (shared by the eye painter and the self-test's coverage target).
bool ensure_source(ID3D11DeviceContext* ctx) {
    if(!ctx || !load_recording()) return false;
    ID3D11Device* dev=nullptr;ctx->GetDevice(&dev);
    if(source && owner==dev) {dev->Release();return true;}
    release_targets();owner=dev;
    D3D11_TEXTURE2D_DESC td{};td.Width=recording.width;td.Height=recording.height;
    td.MipLevels=td.ArraySize=td.SampleDesc.Count=1;
    td.Format=DXGI_FORMAT_R8_UNORM;td.BindFlags=D3D11_BIND_SHADER_RESOURCE;
    bool ok=SUCCEEDED(dev->CreateTexture2D(&td,nullptr,&source)) &&
        SUCCEEDED(dev->CreateShaderResourceView(source,nullptr,&sourceSrv));
    if(ok) {
        frame_inspector::ScopedSuppress quiet;
        ok=recording.decode_frame(0,pixels);
        if(ok) {ctx->UpdateSubresource(source,0,nullptr,pixels.data(),recording.width,0);frameIndex=0;}
    }
    if(!ok) {release_targets();BVR_LOG("[bo bats] replay resource failed; original UI retained");return false;}
    return true;
}
bool ensure_targets(ID3D11DeviceContext* ctx) {
    if(!ensure_source(ctx)) return false;
    unsigned w=checkW,h=checkH;
    if(!w && !vr::output_eye_size(&w,&h) && !vr::recommended_eye_size(&w,&h)) return false;
    if(w<32 || h<32 || w>8192 || h>8192) return false;
    if(mask && outW==w && outH==h) return true;
    drop(maskRtv);drop(mask);outW=outH=0;
    D3D11_TEXTURE2D_DESC td{};td.Width=w;td.Height=h;td.Format=DXGI_FORMAT_R8G8B8A8_UNORM;
    td.MipLevels=td.ArraySize=td.SampleDesc.Count=1;
    td.BindFlags=D3D11_BIND_RENDER_TARGET|D3D11_BIND_SHADER_RESOURCE;
    bool ok=SUCCEEDED(owner->CreateTexture2D(&td,nullptr,&mask)) &&
        SUCCEEDED(owner->CreateRenderTargetView(mask,nullptr,&maskRtv));
    if(ok) {
        frame_inspector::ScopedSuppress quiet;
        const float clear[4]{};ctx->ClearRenderTargetView(maskRtv,clear);
        ok=blit::mask_black(ctx,maskRtv,w,h,sourceSrv,1.f);
    }
    if(!ok) {drop(maskRtv);drop(mask);BVR_LOG("[bo bats] replay resource/shader failed; original UI retained");return false;}
    outW=w;outH=h;
    BVR_LOG("[bo bats] animated coverage %ux%u -> %ux%u output, independent of DLSS",recording.width,recording.height,w,h);
    return true;
}
bool owns(ID3D11DeviceContext* ctx) {
    const auto now=GetTickCount64();synchronize(now);
    return permitted(now) && playback.owns() && ensure_source(ctx);
}
bool draw(ID3D11DeviceContext* ctx,unsigned,bool,gfx_hud::Decision& d) {
    // BOOTBLACK: what this UI frame is made of, until the first menu is up.
    if(!bootDone.load(std::memory_order_relaxed)) {
        ID3D11ShaderResourceView* srv=nullptr;ctx->PSGetShaderResources(0,1,&srv);
        if(srv) {srv->Release();frameTextured.fetch_add(1);} else frameUntextured.fetch_add(1);
    }
    // Observe the proved UI lane, but NEVER discard the original bat/menu
    // drawing. The floating panel is withheld while the eyes hold the bats.
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
    // EYEBATS: while the eyes hold the bats / black, no floating panel - it would sit over them as a window (City
    // WIPEPANEL). For 1.5 s after, only its coloured UI: the game's own bats may still be finishing on the sheet.
    const uint64_t since=GetTickCount64()-lastEyeMs.load(std::memory_order_relaxed);
    if(since<100) return nullptr;
    auto* ui=gfx_hud::texture(ctx);
    return since<1500 ? filter_ui(ctx,ui):ui;
}
float smooth(float x) {x=std::clamp(x,0.f,1.f);return x*x*(3-2*x);}
bool update_source(ID3D11DeviceContext* ctx,uint64_t elapsed) {
    const auto it=std::upper_bound(recording.times.begin(),recording.times.end(),elapsed);
    const unsigned index=it==recording.times.begin()?0:unsigned(it-recording.times.begin()-1);
    if(index!=frameIndex) {
        if(!recording.decode_frame(index,pixels)) return false;
        ctx->UpdateSubresource(source,0,nullptr,pixels.data(),recording.width,0);frameIndex=index;
    }
    return true;
}
// Finish at an exact clear/opaque endpoint without normalizing away bats
// or shrinking the recorded image's animated gaps into a box.
float tail_of(const wipe::Step& step) {
    return smooth(float(int64_t(step.elapsed)-int64_t(recording.times.back()-120))/120.f);
}
bool render_step(ID3D11DeviceContext* ctx,const wipe::Step& step) {
    frame_inspector::ScopedSuppress quiet;
    const float clear[4]={0,0,0,step.visual==wipe::Visual::Black ? 1.f:0.f};
    ctx->ClearRenderTargetView(maskRtv,clear);
    if(step.visual!=wipe::Visual::Animation) return true;
    if(!update_source(ctx,step.elapsed)) return false;
    const float tail=tail_of(step);
    return blit::mask_black(ctx,maskRtv,outW,outH,sourceSrv,zoom.load(),step.invert,
        step.invert ? 1-tail:1.f,step.invert ? 0.f:tail);
}
// EYEBATS: called by the runtime for every finished headset eye image (after DLSS / AFW / room screen).
std::atomic<ULONGLONG> entryBlackUntil{0}; // ENTRYHOLD
// ENTRYFADE (JJ 2026-10-27 night: "the black is fine, but it needs to fade into the game rather than just cut into it"): after the hold,
// a black layer over the finished eye image fades out over kEntryFadeMs (alpha blend, own tiny pipeline, the caller's state kept).
constexpr ULONGLONG kEntryFadeMs = 1000;
ID3D11VertexShader* fadeVs = nullptr; ID3D11PixelShader* fadePs = nullptr; ID3D11Buffer* fadeCb = nullptr;
ID3D11BlendState* fadeBlend = nullptr; ID3D11RasterizerState* fadeRs = nullptr; ID3D11DepthStencilState* fadeDs = nullptr;
bool fadeFailed = false;
bool fade_ready(ID3D11DeviceContext* ctx) {
    if (fadeVs) return true;
    if (fadeFailed) return false;
    static const char src[] =
        "cbuffer F : register(b0) { float4 a; };\n"
        "float4 vs(uint id : SV_VertexID) : SV_Position { float2 t = float2((id << 1) & 2, id & 2); return float4(t * float2(2, -2) + float2(-1, 1), 0, 1); }\n"
        "float4 ps(float4 p : SV_Position) : SV_Target { return float4(0, 0, 0, a.x); }\n";
    ID3D11Device* dev = nullptr; ctx->GetDevice(&dev);
    ID3DBlob *v = nullptr, *f = nullptr;
    bool ok = dev && SUCCEEDED(D3DCompile(src, sizeof(src) - 1, "bo_entryfade", nullptr, nullptr, "vs", "vs_4_0", 0, 0, &v, nullptr)) &&
              SUCCEEDED(D3DCompile(src, sizeof(src) - 1, "bo_entryfade", nullptr, nullptr, "ps", "ps_4_0", 0, 0, &f, nullptr)) &&
              SUCCEEDED(dev->CreateVertexShader(v->GetBufferPointer(), v->GetBufferSize(), nullptr, &fadeVs)) &&
              SUCCEEDED(dev->CreatePixelShader(f->GetBufferPointer(), f->GetBufferSize(), nullptr, &fadePs));
    if (v) v->Release();
    if (f) f->Release();
    if (ok) {
        D3D11_BUFFER_DESC cd{16, D3D11_USAGE_DEFAULT, D3D11_BIND_CONSTANT_BUFFER, 0, 0, 0};
        D3D11_BLEND_DESC bd{};
        bd.RenderTarget[0].BlendEnable = TRUE;
        bd.RenderTarget[0].SrcBlend = D3D11_BLEND_SRC_ALPHA; bd.RenderTarget[0].DestBlend = D3D11_BLEND_INV_SRC_ALPHA;
        bd.RenderTarget[0].BlendOp = D3D11_BLEND_OP_ADD;
        bd.RenderTarget[0].SrcBlendAlpha = D3D11_BLEND_ZERO; bd.RenderTarget[0].DestBlendAlpha = D3D11_BLEND_ONE;
        bd.RenderTarget[0].BlendOpAlpha = D3D11_BLEND_OP_ADD;
        bd.RenderTarget[0].RenderTargetWriteMask = D3D11_COLOR_WRITE_ENABLE_ALL;
        D3D11_RASTERIZER_DESC rd{}; rd.FillMode = D3D11_FILL_SOLID; rd.CullMode = D3D11_CULL_NONE; rd.DepthClipEnable = TRUE;
        D3D11_DEPTH_STENCIL_DESC dd{};
        ok = SUCCEEDED(dev->CreateBuffer(&cd, nullptr, &fadeCb)) && SUCCEEDED(dev->CreateBlendState(&bd, &fadeBlend)) &&
             SUCCEEDED(dev->CreateRasterizerState(&rd, &fadeRs)) && SUCCEEDED(dev->CreateDepthStencilState(&dd, &fadeDs));
    }
    if (dev) dev->Release();
    fadeFailed = !ok;
    BVR_LOG("[bo] ENTRYFADE %s", ok ? "ready" : "setup FAILED - the hold ends with a cut");
    return ok;
}
void fade_black(ID3D11DeviceContext* ctx, ID3D11RenderTargetView* rtv, uint32_t w, uint32_t h, float alpha) {
    if (!fade_ready(ctx)) return;
    ID3D11RenderTargetView* oRtv[8] = {}; ID3D11DepthStencilView* oDsv = nullptr; ctx->OMGetRenderTargets(8, oRtv, &oDsv);
    ID3D11BlendState* oB = nullptr; FLOAT bf[4]; UINT mask = 0; ctx->OMGetBlendState(&oB, bf, &mask);
    ID3D11DepthStencilState* oD = nullptr; UINT ref = 0; ctx->OMGetDepthStencilState(&oD, &ref);
    ID3D11RasterizerState* oR = nullptr; ctx->RSGetState(&oR);
    UINT nvp = 1; D3D11_VIEWPORT oVp{}; ctx->RSGetViewports(&nvp, &oVp);
    ID3D11VertexShader* oVs = nullptr; ctx->VSGetShader(&oVs, nullptr, nullptr);
    ID3D11HullShader* oHs = nullptr; ctx->HSGetShader(&oHs, nullptr, nullptr);
    ID3D11DomainShader* oDs = nullptr; ctx->DSGetShader(&oDs, nullptr, nullptr);
    ID3D11GeometryShader* oGs = nullptr; ctx->GSGetShader(&oGs, nullptr, nullptr);
    ID3D11PixelShader* oPs = nullptr; ctx->PSGetShader(&oPs, nullptr, nullptr);
    ID3D11Buffer* oCb = nullptr; ctx->PSGetConstantBuffers(0, 1, &oCb);
    ID3D11InputLayout* oIl = nullptr; ctx->IAGetInputLayout(&oIl);
    D3D11_PRIMITIVE_TOPOLOGY oTopo{}; ctx->IAGetPrimitiveTopology(&oTopo);
    const float a[4] = {alpha, 0, 0, 0};
    ctx->UpdateSubresource(fadeCb, 0, nullptr, a, 0, 0);
    ctx->OMSetRenderTargets(1, &rtv, nullptr);
    ctx->OMSetBlendState(fadeBlend, nullptr, 0xFFFFFFFF); ctx->OMSetDepthStencilState(fadeDs, 0); ctx->RSSetState(fadeRs);
    D3D11_VIEWPORT vp{0, 0, float(w), float(h), 0, 1}; ctx->RSSetViewports(1, &vp);
    ctx->IASetInputLayout(nullptr); ctx->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    ctx->VSSetShader(fadeVs, nullptr, 0); ctx->HSSetShader(nullptr, nullptr, 0); ctx->DSSetShader(nullptr, nullptr, 0);
    ctx->GSSetShader(nullptr, nullptr, 0); ctx->PSSetShader(fadePs, nullptr, 0); ctx->PSSetConstantBuffers(0, 1, &fadeCb);
    ctx->Draw(3, 0);
    ctx->OMSetRenderTargets(8, oRtv, oDsv); ctx->OMSetBlendState(oB, bf, mask); ctx->OMSetDepthStencilState(oD, ref); ctx->RSSetState(oR);
    if (nvp) ctx->RSSetViewports(1, &oVp);
    ctx->VSSetShader(oVs, nullptr, 0); ctx->HSSetShader(oHs, nullptr, 0); ctx->DSSetShader(oDs, nullptr, 0); ctx->GSSetShader(oGs, nullptr, 0);
    ctx->PSSetShader(oPs, nullptr, 0); ctx->PSSetConstantBuffers(0, 1, &oCb); ctx->IASetInputLayout(oIl); ctx->IASetPrimitiveTopology(oTopo);
    IUnknown* all[] = {oDsv, oB, oD, oR, oVs, oHs, oDs, oGs, oPs, oCb, oIl};
    for (IUnknown* u : all) if (u) u->Release();
    for (auto* r : oRtv) if (r) r->Release();
}
bool paint_eye(ID3D11DeviceContext* ctx,int,ID3D11RenderTargetView* rtv,uint32_t w,uint32_t h,
               float shiftX,float shiftY,bool projection,int64_t frame) {
    const auto now=GetTickCount64();
    if(projection) lastProjectionMs.store(now);
    synchronize(now);
    if(ctx && rtv && now<entryBlackUntil.load()) { // ENTRYHOLD: still the loading screen's black while the world loads in
        frame_inspector::ScopedSuppress quiet;
        const float k[4]={0,0,0,1}; ctx->ClearRenderTargetView(rtv,k); return true;
    }
    if(ctx && rtv && entryBlackUntil.load() && now<entryBlackUntil.load()+kEntryFadeMs) { // ENTRYFADE: then fade into the game
        frame_inspector::ScopedSuppress quiet;
        fade_black(ctx,rtv,w,h,1.0f-float(now-entryBlackUntil.load())/float(kEntryFadeMs));
        return true;
    }
    if(!ctx || !rtv || !permitted(now)) return false;
    frame_inspector::ScopedSuppress quiet;
    if(!bootDone.load() && events.load()) {bootDone.store(true);BVR_LOG("[bo bats] BOOTBLACK done (title exit)");}
    if(!bootDone.load() && projection && frameUntextured.load() && !frameTextured.load()) {
        if(now>=bootBlackUntil && bootBlackLogs++<4)
            BVR_LOG("[bo bats] BOOTBLACK: the game's full-screen black before the first menu -> whole view black");
        bootBlackUntil=now+150;
    }
    bool black=!bootDone.load() && now<bootBlackUntil;
    if(!black) {
        if(!ensure_source(ctx)) return false;
        // One step per XR frame: both eyes of a pair show the same picture, whichever is written first.
        if(frame!=lastFrame || now-lastTickMs>40) {
            lastFrame=frame;lastTickMs=now;
            // FASTTRAVEL (JJ 2026-10-26: fast travel "played a cutscene but I saw nothing in the headset it was just black"; LIVE
            // log 23:20:59-23:21:26: bats -> black, then an in-engine cinematic on the cinematic screen, never back to
            // projection = black held until quit). A 3D world drawn onto the cinematic screen ends the black hold like
            // projection does; loading / FMV screens draw no world pass and stay black.
            // FASTTRAVEL3 (JJ 23:31 "It's still not working"; LIVE 23:31:01: letterbox ON, world pass OFF, "3D scene NOT seen"):
            // the fast-travel cutscene is a full-screen VIDEO (IsPlayingFullScreenMovie TRUE), not a 3D scene. The flat game
            // shows it, so any real picture on the cinematic screen ends the hold: a 3D scene, or the letterboxed video (the
            // pixel watch reports bars only around picture content; an all-black frame reads as no bars).
            const bool cineVideo=!projection && vr::cinematic_active() && hud::letterbox(nullptr,nullptr);
            const bool cineWorld=!projection && vr::cinematic_active() && (hud::world_pass() || cineVideo);
            const bool held=playback.phase==wipe::Phase::Hold;
            lastStep=playback.tick(now,projection || cineWorld,recording.times.back());
            if(held && cineWorld && playback.phase==wipe::Phase::Reveal)
                BVR_LOG("[bo bats] reveal onto the cinematic screen (%s)",cineVideo?"letterboxed video / cinematic":"3D scene");
        }
        if(lastStep.visual==wipe::Visual::None) return false;
        black=lastStep.visual==wipe::Visual::Black;
    }
    bool ok=true;
    if(black) {const float k[4]={0,0,0,1};ctx->ClearRenderTargetView(rtv,k);}
    else {
        const float tail=tail_of(lastStep);
        ok=update_source(ctx,lastStep.elapsed) &&
            blit::mask_black(ctx,rtv,w,h,sourceSrv,eyeZoom.load(),lastStep.invert,
                lastStep.invert ? 1-tail:1.f,lastStep.invert ? 0.f:tail,shiftX,shiftY);
    }
    if(ok) {lastEyeMs.store(now);thisFrame=true;eyePaints.fetch_add(1);}
    return ok;
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
    // BOOTBLACK ends for good once the first menu has drawn textured UI for 60 frames in the 3D view.
    if(!bootDone.load()) {
        const bool projNow=GetTickCount64()-lastProjectionMs.load()<100;
        if(frameTextured.load() && projNow) {
            if(++texturedRun>=60) {bootDone.store(true);BVR_LOG("[bo bats] BOOTBLACK done (first menu drawn)");}
        } else if(frameUntextured.load()) texturedRun=0;
    }
    frameTextured.store(0);frameUntextured.store(0);
    // RELEASE 0.1.0: the automatic bat_layers_* sample of the first two events is removed; "Record next bat transition"
    // in the TESTS drawer (`bo bats record`) saves the same pictures on request.
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
    swprintf_s(v,L"%.2f",eyeZoom.load());WritePrivateProfileStringW(L"bats",L"eye_enlargement",v,path);
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
    GetPrivateProfileStringW(L"bats",L"eye_enlargement",L"1.6",v,48,path);
    eyeZoom.store(std::clamp(float(wcstod(v,nullptr)),1.f,2.5f));
    gfx_hud::set_draw_override(draw,nullptr,end,nullptr);
    gfx_hud::set_before_capture_hook(prepare);
    // EYEBATS: painted into the eye images; the separate quad layer (shown as a window) is no longer submitted.
    vr::set_transition_eye_painter(paint_eye);vr::set_transition_distance(distance.load());
    vr::set_hud_texture_provider(hud_texture);
    BVR_LOG("[bo bats] Origins event hooks ready, id %u; bats + black painted into both eyes (EYEBATS), %.2f m, %.2fx; BOOTBLACK on",
        batEvent,distance.load(),eyeZoom.load());
    return true;
}

void save_capture(ID3D11DeviceContext* ctx,const wchar_t* dir) {
    wchar_t path[MAX_PATH]{};
    if(auto* hud=gfx_hud::texture(ctx)) {swprintf_s(path,L"%s\\hud_panel.bmp",dir);gfx_hud::save_texture_bmp(ctx,hud,path);}
    if(thisFrame && colourUi) {swprintf_s(path,L"%s\\preserved_menu.bmp",dir);gfx_hud::save_texture_bmp(ctx,colourUi,path);}
    if(thisFrame && mask) {swprintf_s(path,L"%s\\transition_coverage.bmp",dir);gfx_hud::save_texture_bmp(ctx,mask,path);}
    swprintf_s(path,L"%s\\hud_last_frames.txt",dir);gfx_hud::save_hud_ring(path);
    swprintf_s(path,L"%s\\transition.txt",dir);FILE* f=nullptr;_wfopen_s(&f,path,L"w");
    if(f) {fprintf(f,"event=%u events=%u phase=%u visual=%u elapsed=%llu frame=%u invert=%d\nsource=%ux%u output=%ux%u distance=%.2f eye_enlargement=%.2f\nEYEBATS: bats/black painted into each eye image; the eye BMPs include them.\n",
        batEvent,events.load(),unsigned(playback.phase),unsigned(lastStep.visual),lastStep.elapsed,frameIndex,lastStep.invert,
        recording.width,recording.height,outW,outH,distance.load(),eyeZoom.load());fclose(f);}
    swprintf_s(path,L"%s\\submission.txt",dir);_wfopen_s(&f,path,L"w");
    if(f) {fprintf(f,"EYEBATS: painted into the eye images (eye_left/eye_right.bmp here); eye paints %u, runtime eye draws %u\n",
        eyePaints.load(),vr::transition_eye_draws());fclose(f);}
}
void status() {
    BVR_LOG("[bo bats] hooks=%d on=%d event=%u signals=%u preserved-draws=%u phase=%u eye-enlargement=%.2f distance=%.2f",
        liveHooks,enabled.load(),batEvent,events.load(),draws.load(),unsigned(playback.phase),eyeZoom.load(),distance.load());
    BVR_LOG("[bo bats] EYEBATS eye paints=%u (runtime %u); BOOTBLACK %s",eyePaints.load(),vr::transition_eye_draws(),
        bootDone.load() ? "done":"waiting for the first menu");
}
bool command(const char* args) {
    if(!strcmp(args,"on")) enabled.store(true);
    else if(!strcmp(args,"off")) enabled.store(false);
    else if(!strcmp(args,"original")) originalUntil.store(GetTickCount64()+15000);
    else if(!strcmp(args,"record")) recordNext.store(true);
    else if(!strcmp(args,"probe")) { probeFastTravel.store(true); BVR_LOG("[bo bats] FTPROBE armed for the next fast travel"); }
    else if(!strcmp(args,"status")) { status();return true; }
    else return false;
    settings_save();return true;
}
// RELEASE 0.1.0: player settings here (MENUS AND SCREENS drawer), the test buttons in draw_test_ui (TESTS drawer).
void draw_ui() {
    bool on=enabled.load();if(ImGui::Checkbox("bat transitions over the whole view",&on)) {enabled.store(on);settings_save();}
    float d=distance.load(),z=eyeZoom.load();bool changed=false;
    changed |= overlay::SliderStep("bat transition distance (m)",&d,.4f,2.f);
    changed |= overlay::SliderStep("bat transition size",&z,1.f,2.5f);
    if(changed) {distance.store(d);eyeZoom.store(z);vr::set_transition_distance(d);settings_save();}
    if(!liveHooks) ImGui::TextDisabled("   the game's own bat transitions are shown (this game version is not supported)");
}
void draw_test_ui() {
    if(ImGui::Button("Original bat transitions for 15 s")) originalUntil.store(GetTickCount64()+15000);
    ImGui::SameLine();
    if(ImGui::Button("Record next bat transition")) recordNext.store(true);
    const auto now=GetTickCount64(),until=originalUntil.load();
    if(now<until) ImGui::Text("   original bat transitions: %u seconds left",unsigned((until-now+999)/1000));
    if(recordNext.load()) ImGui::TextUnformatted("   waiting for the next bat transition");
    ImGui::TextDisabled("bat transitions %u, painted into the eyes %u times",events.load(),eyePaints.load());
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
        // EYEBATS: the per-eye shift moves the picture's centre exactly; BOOTBLACK blackens a whole eye image.
        {
            ID3D11Texture2D *eyeA=nullptr,*eyeB=nullptr;ID3D11RenderTargetView *rtvA=nullptr,*rtvB=nullptr;
            td.Format=DXGI_FORMAT_R8G8B8A8_UNORM;
            bool eyeOk=SUCCEEDED(dev->CreateTexture2D(&td,nullptr,&eyeA)) && SUCCEEDED(dev->CreateRenderTargetView(eyeA,nullptr,&rtvA)) &&
                SUCCEEDED(dev->CreateTexture2D(&td,nullptr,&eyeB)) && SUCCEEDED(dev->CreateRenderTargetView(eyeB,nullptr,&rtvB));
            check(eyeOk,"eye image fixture");
            if(eyeOk) {
                const float white[4]={1,1,1,1};ctx->ClearRenderTargetView(rtvA,white);ctx->ClearRenderTargetView(rtvB,white);
                check(update_source(ctx,recording.times[30]) && blit::mask_black(ctx,rtvA,96,104,sourceSrv,1.f) &&
                    blit::mask_black(ctx,rtvB,96,104,sourceSrv,1.f,false,1.f,0.f,.25f,0.f),"shifted eye paint");
                unsigned same=0,tested=0,differs=0;unsigned char a[4]{},b[4]{};
                for(unsigned y=4;y<104;y+=10) for(unsigned x=4;x<70;x+=6) {
                    if(!pixel_at(ctx,eyeA,a,x,y) || !pixel_at(ctx,eyeB,b,x+24,y)) continue;
                    ++tested;same+=abs(int(a[0])-int(b[0]))<=2;
                    unsigned char c2[4]{};if(pixel_at(ctx,eyeB,c2,x,y)) differs+=abs(int(a[0])-int(c2[0]))>40;
                }
                check(tested && same==tested,"eye shift moves the bats by exactly the requested quarter width");
                check(differs>0,"shifted and unshifted eye pictures differ");
                ctx->ClearRenderTargetView(rtvA,white);
                const bool wasDone=bootDone.load();const unsigned wasEvents=events.load(),wasApplied=appliedEvents;
                bootDone.store(false);events.store(0);appliedEvents=0;bootBlackUntil=0;
                frameUntextured.store(1);frameTextured.store(0);
                check(paint_eye(ctx,0,rtvA,96,104,.1f,0,true,7),"BOOTBLACK paints the eye");
                for(auto xy : {std::pair<unsigned,unsigned>{0,0},{95,0},{0,103},{95,103},{48,52}})
                    check(pixel_at(ctx,eyeA,c,xy.first,xy.second) && c[0]==0 && c[1]==0 && c[2]==0,"BOOTBLACK fills the whole eye");
                frameUntextured.store(0);frameTextured.store(1);bootBlackUntil=0;ctx->ClearRenderTargetView(rtvA,white);
                check(!paint_eye(ctx,0,rtvA,96,104,0,0,true,8) && pixel_at(ctx,eyeA,c,48,52) && c[0]==255,
                    "textured menu frame is not blackened");
                frameTextured.store(0);bootDone.store(wasDone);events.store(wasEvents);appliedEvents=wasApplied;
            }
            drop(rtvA);drop(rtvB);drop(eyeA);drop(eyeB);
        }
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
void hold_black(unsigned ms) { entryBlackUntil.store(GetTickCount64() + ms); }
}
