// AIMDEPTH - see aim_depth.h. At the left eye's tone-map step (the first full-screen draw into an 8-bit eye-size target that reads
// the fp16 scene; the scene depth is complete then and no longer bound for writing) one pixel shader reads the depth texel under the
// eye's centre ray into a 1x1 R32F target, copied to a staging texture and read back one eye picture later (never waits).
#include "aim_depth.h"
#include "core/gfx/frame_inspector.h"
#include "core/gfx/gfx_hud.h"
#include "core/util/log.h"
#include <windows.h>
#include <d3d11.h>
#include <d3dcompiler.h>
#include <atomic>
#include <cmath>
#include <cstdio>
#include <mutex>

namespace bvr::bo::aim_depth {
namespace {
struct Eye { float px = 0, py = 0, m22 = 0, m32 = 0, o[3] = {}, f[3] = {}; bool valid = false; };
std::mutex g_eyeLock;
Eye g_eye;                 // left eye, latest (game thread writes)
Eye g_passEye;             // the left eye of the pass being read (render thread)
std::mutex g_ptLock;
float g_pt[3] = {}; ULONGLONG g_ptMs = 0; bool g_ptValid = false;
std::atomic<unsigned> g_reads{0}, g_sky{0}, g_near{0}, g_pending{0}, g_failed{0};
std::atomic<float> g_lastDist{0};

const char kHlsl[] = R"(
cbuffer P : register(b0) { int4 at; };
Texture2D<float> D : register(t0);
float4 vs_main(uint id : SV_VertexID) : SV_Position {
    float2 t = float2((id << 1) & 2, id & 2);
    return float4(t * float2(2, -2) + float2(-1, 1), 0, 1);
}
float4 ps_main(float4 p : SV_Position) : SV_Target { return D.Load(int3(at.xy, 0)).xxxx; }
)";
ID3D11VertexShader* g_vs = nullptr; ID3D11PixelShader* g_ps = nullptr; ID3D11Buffer* g_cb = nullptr;
ID3D11Texture2D* g_one = nullptr; ID3D11RenderTargetView* g_oneRtv = nullptr; ID3D11Texture2D* g_staging = nullptr;
ID3D11BlendState* g_blend = nullptr; ID3D11RasterizerState* g_rs = nullptr; ID3D11DepthStencilState* g_ds = nullptr;
ID3D11Resource* g_depthRes = nullptr; ID3D11ShaderResourceView* g_depthSrv = nullptr; // SRV cached per depth texture
bool g_failedSetup = false;
bool g_doneThisPass = true, g_isLeft = false, g_waiting = false;
Eye g_copyEye; // the eye the waiting read belongs to

bool setup(ID3D11Device* dev) {
    if (g_vs) return true;
    if (g_failedSetup) return false;
    ID3DBlob *v = nullptr, *p = nullptr;
    bool ok = SUCCEEDED(D3DCompile(kHlsl, sizeof(kHlsl) - 1, "bo_aimdepth", nullptr, nullptr, "vs_main", "vs_4_0", 0, 0, &v, nullptr)) &&
              SUCCEEDED(D3DCompile(kHlsl, sizeof(kHlsl) - 1, "bo_aimdepth", nullptr, nullptr, "ps_main", "ps_4_0", 0, 0, &p, nullptr)) &&
              SUCCEEDED(dev->CreateVertexShader(v->GetBufferPointer(), v->GetBufferSize(), nullptr, &g_vs)) &&
              SUCCEEDED(dev->CreatePixelShader(p->GetBufferPointer(), p->GetBufferSize(), nullptr, &g_ps));
    if (v) v->Release();
    if (p) p->Release();
    if (ok) {
        D3D11_BUFFER_DESC cd{16, D3D11_USAGE_DEFAULT, D3D11_BIND_CONSTANT_BUFFER, 0, 0, 0};
        D3D11_TEXTURE2D_DESC td{};
        td.Width = td.Height = 1; td.MipLevels = td.ArraySize = 1; td.Format = DXGI_FORMAT_R32_FLOAT; td.SampleDesc.Count = 1;
        td.Usage = D3D11_USAGE_DEFAULT; td.BindFlags = D3D11_BIND_RENDER_TARGET;
        D3D11_TEXTURE2D_DESC sd = td; sd.Usage = D3D11_USAGE_STAGING; sd.BindFlags = 0; sd.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
        D3D11_BLEND_DESC bd{}; bd.RenderTarget[0].RenderTargetWriteMask = D3D11_COLOR_WRITE_ENABLE_ALL;
        D3D11_RASTERIZER_DESC rd{}; rd.FillMode = D3D11_FILL_SOLID; rd.CullMode = D3D11_CULL_NONE; rd.DepthClipEnable = TRUE;
        D3D11_DEPTH_STENCIL_DESC dd{};
        ok = SUCCEEDED(dev->CreateBuffer(&cd, nullptr, &g_cb)) && SUCCEEDED(dev->CreateTexture2D(&td, nullptr, &g_one)) &&
             SUCCEEDED(dev->CreateRenderTargetView(g_one, nullptr, &g_oneRtv)) && SUCCEEDED(dev->CreateTexture2D(&sd, nullptr, &g_staging)) &&
             SUCCEEDED(dev->CreateBlendState(&bd, &g_blend)) && SUCCEEDED(dev->CreateRasterizerState(&rd, &g_rs)) &&
             SUCCEEDED(dev->CreateDepthStencilState(&dd, &g_ds));
    }
    g_failedSetup = !ok;
    BVR_LOG("[aimdepth] %s", ok ? "ready: the head's aim point from the left eye's depth" : "setup FAILED - aim stays parallel to the game camera");
    return ok;
}
ID3D11ShaderResourceView* depth_srv(ID3D11Device* dev, ID3D11Resource* r) {
    if (r == g_depthRes && g_depthSrv) return g_depthSrv;
    if (g_depthSrv) { g_depthSrv->Release(); g_depthSrv = nullptr; }
    g_depthRes = r;
    D3D11_SHADER_RESOURCE_VIEW_DESC vd{};
    vd.Format = DXGI_FORMAT_R24_UNORM_X8_TYPELESS; vd.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D; vd.Texture2D.MipLevels = 1;
    if (FAILED(dev->CreateShaderResourceView(r, &vd, &g_depthSrv))) g_depthSrv = nullptr;
    return g_depthSrv;
}
// The waiting read, if the GPU has it: depth -> view distance along the centre ray -> world point.
void collect(ID3D11DeviceContext* ctx) {
    if (!g_waiting) return;
    D3D11_MAPPED_SUBRESOURCE m{};
    const HRESULT hr = ctx->Map(g_staging, 0, D3D11_MAP_READ, D3D11_MAP_FLAG_DO_NOT_WAIT, &m);
    if (hr == DXGI_ERROR_WAS_STILL_DRAWING) { g_pending.fetch_add(1, std::memory_order_relaxed); return; }
    g_waiting = false;
    if (FAILED(hr)) { g_failed.fetch_add(1, std::memory_order_relaxed); return; }
    const float d = *static_cast<const float*>(m.pData);
    ctx->Unmap(g_staging, 0);
    const Eye& e = g_copyEye;
    const float den = d - e.m22;
    float z = std::fabs(den) > 1e-7f ? e.m32 / den : 1e9f;
    if (!std::isfinite(z) || z <= 0 || z > 1e6f) { g_sky.fetch_add(1, std::memory_order_relaxed); z = 1e6f; } // sky / far plane
    if (z < 100.0f) { g_near.fetch_add(1, std::memory_order_relaxed); return; } // his own arm / cape in front of the lens
    std::lock_guard<std::mutex> lk(g_ptLock);
    for (int i = 0; i < 3; ++i) g_pt[i] = e.o[i] + e.f[i] * z;
    g_ptMs = GetTickCount64(); g_ptValid = true;
    g_lastDist.store(z, std::memory_order_relaxed);
    g_reads.fetch_add(1, std::memory_order_relaxed);
}
bool is_ldr(DXGI_FORMAT f) {
    return f == DXGI_FORMAT_R8G8B8A8_UNORM || f == DXGI_FORMAT_R8G8B8A8_UNORM_SRGB || f == DXGI_FORMAT_R8G8B8A8_TYPELESS ||
           f == DXGI_FORMAT_B8G8R8A8_UNORM || f == DXGI_FORMAT_B8G8R8A8_UNORM_SRGB || f == DXGI_FORMAT_R10G10B10A2_UNORM;
}
bool desc_of(ID3D11View* v, D3D11_TEXTURE2D_DESC* d) {
    if (!v) return false;
    ID3D11Resource* r = nullptr; v->GetResource(&r); if (!r) return false;
    D3D11_RESOURCE_DIMENSION dim{}; r->GetType(&dim);
    const bool ok = dim == D3D11_RESOURCE_DIMENSION_TEXTURE2D;
    if (ok) static_cast<ID3D11Texture2D*>(r)->GetDesc(d);
    r->Release();
    return ok;
}
} // namespace

void set_eye(int eyeSign, float px, float py, float m22, float m32, const float origin[3], const float forward[3]) {
    if (eyeSign >= 0) return; // the left eye only
    std::lock_guard<std::mutex> lk(g_eyeLock);
    g_eye.px = px; g_eye.py = py; g_eye.m22 = m22; g_eye.m32 = m32;
    for (int i = 0; i < 3; ++i) { g_eye.o[i] = origin[i]; g_eye.f[i] = forward[i]; }
    g_eye.valid = std::isfinite(px) && std::isfinite(py) && std::isfinite(m22) && std::isfinite(m32);
}

void note_eye_pass(int eyeSign) {
    g_isLeft = eyeSign < 0;
    g_doneThisPass = !g_isLeft;
    if (g_isLeft) { std::lock_guard<std::mutex> lk(g_eyeLock); g_passEye = g_eye; }
}

void before_draw(ID3D11DeviceContext* ctx, unsigned vertexCount) {
    if (g_doneThisPass || vertexCount != 3 || !g_passEye.valid) return;
    ID3D11RenderTargetView* rtv = nullptr;
    ctx->OMGetRenderTargets(1, &rtv, nullptr);
    D3D11_TEXTURE2D_DESC rd{};
    const bool rtOk = desc_of(rtv, &rd);
    if (rtv) rtv->Release();
    if (!rtOk || !is_ldr(rd.Format) || rd.Width < 800) return;
    ID3D11ShaderResourceView* s0 = nullptr;
    ctx->PSGetShaderResources(0, 1, &s0);
    D3D11_TEXTURE2D_DESC sd{};
    const bool sceneIn = desc_of(s0, &sd) && sd.Format == DXGI_FORMAT_R16G16B16A16_FLOAT && sd.Width >= 800;
    if (s0) s0->Release();
    if (!sceneIn) return;
    g_doneThisPass = true; // the tone-map step of the left eye
    ID3D11Device* dev = nullptr;
    ctx->GetDevice(&dev);
    ID3D11Resource* depth = bvr::gfx_hud::scene_depth();
    ID3D11ShaderResourceView* dsrv = dev && depth && setup(dev) ? depth_srv(dev, depth) : nullptr;
    if (dev) dev->Release();
    if (!dsrv) return;
    bvr::frame_inspector::ScopedSuppress quiet;
    collect(ctx); // last time's answer first
    if (g_waiting) return;
    D3D11_TEXTURE2D_DESC dd{};
    static_cast<ID3D11Texture2D*>(depth)->GetDesc(&dd);
    const int at[4] = {int(std::fmin(std::fmax(g_passEye.px, 0.0f), float(dd.Width - 1))),
                       int(std::fmin(std::fmax(g_passEye.py, 0.0f), float(dd.Height - 1))), 0, 0};
    // save / set / draw / restore (the game's draw then runs on its own state)
    ID3D11RenderTargetView* oRtv[8] = {}; ID3D11DepthStencilView* oDsv = nullptr;
    ctx->OMGetRenderTargets(8, oRtv, &oDsv);
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
    ID3D11ShaderResourceView* oSrv = nullptr; ctx->PSGetShaderResources(0, 1, &oSrv);
    ID3D11InputLayout* oIl = nullptr; ctx->IAGetInputLayout(&oIl);
    D3D11_PRIMITIVE_TOPOLOGY oTopo{}; ctx->IAGetPrimitiveTopology(&oTopo);

    ctx->UpdateSubresource(g_cb, 0, nullptr, at, 0, 0);
    ctx->OMSetRenderTargets(1, &g_oneRtv, nullptr);
    ctx->OMSetBlendState(g_blend, nullptr, 0xFFFFFFFF); ctx->OMSetDepthStencilState(g_ds, 0); ctx->RSSetState(g_rs);
    D3D11_VIEWPORT vp{0, 0, 1, 1, 0, 1}; ctx->RSSetViewports(1, &vp);
    ctx->IASetInputLayout(nullptr); ctx->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    ctx->VSSetShader(g_vs, nullptr, 0); ctx->HSSetShader(nullptr, nullptr, 0); ctx->DSSetShader(nullptr, nullptr, 0);
    ctx->GSSetShader(nullptr, nullptr, 0); ctx->PSSetShader(g_ps, nullptr, 0); ctx->PSSetConstantBuffers(0, 1, &g_cb);
    ctx->PSSetShaderResources(0, 1, &dsrv);
    ctx->Draw(3, 0);
    ID3D11ShaderResourceView* none = nullptr; ctx->PSSetShaderResources(0, 1, &none);
    ctx->CopyResource(g_staging, g_one);
    g_copyEye = g_passEye; g_waiting = true;

    ctx->OMSetRenderTargets(8, oRtv, oDsv); ctx->OMSetBlendState(oB, bf, mask); ctx->OMSetDepthStencilState(oD, ref); ctx->RSSetState(oR);
    if (nvp) ctx->RSSetViewports(1, &oVp);
    ctx->VSSetShader(oVs, nullptr, 0); ctx->HSSetShader(oHs, nullptr, 0); ctx->DSSetShader(oDs, nullptr, 0); ctx->GSSetShader(oGs, nullptr, 0);
    ctx->PSSetShader(oPs, nullptr, 0); ctx->PSSetConstantBuffers(0, 1, &oCb); ctx->PSSetShaderResources(0, 1, &oSrv);
    ctx->IASetInputLayout(oIl); ctx->IASetPrimitiveTopology(oTopo);
    IUnknown* all[] = {oDsv, oB, oD, oR, oVs, oHs, oDs, oGs, oPs, oCb, oSrv, oIl};
    for (IUnknown* u : all) if (u) u->Release();
    for (auto* r : oRtv) if (r) r->Release();
}

bool point(float out[3]) {
    std::lock_guard<std::mutex> lk(g_ptLock);
    if (!g_ptValid || GetTickCount64() - g_ptMs > 150) return false;
    for (int i = 0; i < 3; ++i) out[i] = g_pt[i];
    return true;
}

void status_line(char* out, unsigned n) {
    snprintf(out, n, "aimdepth: reads %u (sky %u, too near %u), waiting %u, failed %u, last distance %.0f UU", g_reads.load(), g_sky.load(),
             g_near.load(), g_pending.load(), g_failed.load(), g_lastDist.load());
}
} // namespace bvr::bo::aim_depth
