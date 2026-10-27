// DETVR for Origins: see detective.h. City's code (batman_city vision.cpp DETVR) is the method; only the shader identity is new.
#include "detective.h"
#include "core/ui/overlay.h"
#include "core/util/log.h"
#include "core/vr/openxr_runtime.h"
#include <d3d11.h>
#include <d3dcompiler.h>
#include <MinHook.h>
#include <imgui.h>
#include <windows.h>
#include <algorithm>
#include <atomic>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <string>
#include <unordered_map>

namespace bvr::bo::detective {
namespace {
constexpr unsigned long long kCityDetective = 0x9f1079e5fc4a7d14ull; // City's material (DXBC checksum, first 8 bytes)
// DETVR2 (JJ 2026-10-28: City's rebuild "works, but ... when first entering detective mode ... it's splitting the screen into four
// quadrants very briefly ... The same thing happens when exiting"; and "strange vertical banding in the object border highlights that
// slowly moves from right to left ... I don't think it's fusing in both eyes"). LIVE 23:55: 20 Origins pixel shaders share City's
// detective inputs; three were drawn full-screen (611c06c683aff775 first - the enter / exit effect -, 6630ccfd39543ce7, 3c05185aeabc50a2)
// and the test swapped City's material onto ALL of them = the quadrants. Only 6630ccfd39543ce7 has City's layout (7 pictures, cb0[15]):
// Origins' detective material (disassembly: City's + flag at cb2[5].x, a final colour step x cb2[4].w + cb2[4].xyz). Only it is swapped,
// for its own rebuild. The border highlight's scrolling stripe picture t6 was sampled at each eye's own screen x (both in City's
// material and here) = bands that differ per eye and crawl; it now uses the shared overlay position too.
constexpr unsigned long long kOriginsDetective = 0x6630ccfd39543ce7ull; // checksum bytes in file order (log / file name)
// SCANVR (JJ 2026-10-28, evidence scanner: "not much seems to move besides what looks like some sort of scan line overlay which seems to
// be attached to your face"; SCREENMARK 00:26: the only material drawn in the scanner and not in normal play = 13572b59fe76fadf): a
// smaller cousin of the detective material - t2 honeycomb + vignette at uv x2, t3 scan lines at (uv.x, uv.y x 24), t4 grain at uv x4 +
// cb0[8].zw, all at each eye's own screen position. Rebuilt the same way (overlays at the shared position); captures 25572765 /
// 25600062 show the scanner view itself head-tracked in stereo.
constexpr unsigned long long kOriginsScanner = 0x13572b59fe76fadfull;
// SCANHUD (JJ 2026-10-28: "the head tracking in the evidence mode only works when you hold down left trigger"; LIVE 00:33:08-00:33:45:
// look steering ran the whole time, so the game's scan direction followed the head - but the scanner's centre target is HUD, and the
// HUD rides the head only while the trigger is held (HUDAIM)). The scanner material's last draw time; scene.cpp keeps the HUD on the
// head while it is recent.
std::atomic<ULONGLONG> g_scanDrawMs{0};
// City's material's pixel inputs (re/detvig_9f1079e5fc4a7d14.dxbc): COLOR0, TEXCOORD0, TEXCOORD4, TEXCOORD6, TEXCOORD5, SV_IsFrontFace.
const char* const kSig[] = {"COLOR0", "TEXCOORD0", "TEXCOORD4", "TEXCOORD6", "TEXCOORD5", "SV_IsFrontFace0"};
constexpr int kMaxCand = 32;
struct Cand { ID3D11PixelShader* ps; unsigned long long chk; std::atomic<unsigned> draws; int kind; }; // kind 0 detective, 1 scanner
Cand g_cand[kMaxCand];
std::atomic<int> g_candN{0};
std::atomic<bool> g_tryCity{true};       // DETVR2: "detective overlay fused for both eyes" (saved by scene.cpp, [hud])
std::atomic<int> g_passEye{0};
std::atomic<unsigned> g_created{0}, g_swapped{0};
// DXBC input signature names, "NAMEindex" each; false = not a shader blob.
bool input_names(const uint8_t* code, size_t len, std::string* names, int* n, int max) {
    *n = 0;
    if (len < 32 || memcmp(code, "DXBC", 4)) return false;
    uint32_t chunks = 0;
    memcpy(&chunks, code + 28, 4);
    for (uint32_t i = 0; i < chunks && 32 + 4 * i + 4 <= len; ++i) {
        uint32_t off = 0;
        memcpy(&off, code + 32 + 4 * i, 4);
        if (off + 8 > len || memcmp(code + off, "ISGN", 4)) continue;
        const uint8_t* c = code + off + 8;
        uint32_t size = 0, count = 0;
        memcpy(&size, code + off + 4, 4);
        memcpy(&count, c, 4);
        for (uint32_t k = 0; k < count && *n < max && 8 + 24 * k + 24 <= size; ++k) {
            uint32_t nameOff = 0, idx = 0;
            memcpy(&nameOff, c + 8 + 24 * k, 4);
            memcpy(&idx, c + 8 + 24 * k + 4, 4);
            if (nameOff >= size) return false;
            const char* s = reinterpret_cast<const char*>(c + nameOff);
            names[(*n)++] = std::string(s, strnlen(s, size - nameOff)) + std::to_string(idx);
        }
        return true;
    }
    return false;
}

using CreatePsFn = HRESULT(STDMETHODCALLTYPE*)(ID3D11Device*, const void*, SIZE_T, ID3D11ClassLinkage*, ID3D11PixelShader**);
CreatePsFn g_createPsOrig = nullptr;
HRESULT STDMETHODCALLTYPE create_ps_detour(ID3D11Device* d, const void* code, SIZE_T len, ID3D11ClassLinkage* cl,
                                           ID3D11PixelShader** out) {
    const HRESULT hr = g_createPsOrig(d, code, len, cl, out);
    g_created.fetch_add(1, std::memory_order_relaxed);
    if (FAILED(hr) || !out || !*out || !code || len < 32 || len > (1u << 20)) return hr;
    std::string names[16];
    int n = 0;
    if (!input_names(static_cast<const uint8_t*>(code), len, names, &n, 16) || n != 6) return hr;
    for (int i = 0; i < 6; ++i)
        if (names[i] != kSig[i]) return hr;
    unsigned long long chk = 0;
    memcpy(&chk, static_cast<const uint8_t*>(code) + 4, 8);
    const unsigned long long id = _byteswap_uint64(chk);
    if (id != kOriginsDetective && id != kOriginsScanner) return hr; // DETVR2 / SCANVR: these two materials only
    const int k = g_candN.load(std::memory_order_relaxed);
    if (k >= kMaxCand) return hr;
    g_cand[k].ps = *out;
    g_cand[k].chk = chk;
    g_cand[k].kind = id == kOriginsScanner ? 1 : 0;
    g_candN.store(k + 1, std::memory_order_release);
    BVR_LOG("[bo] DETVR: Origins' %s material created (%016llx, %u bytes)", g_cand[k].kind ? "evidence scanner" : "detective", id,
            unsigned(len));
    return hr;
}

// Origins' detective material 6630ccfd39543ce7, rebuilt from its disassembly (City's kDetVrHlsl + the Origins differences), with the
// overlay textures (t2 scanlines, t3 honeycomb + vignette, t4 / t5 grain, DETVR2: t6 border stripes) at the shared position ov.
const char kDetVrHlsl[] = R"HLSL(
cbuffer CB0 : register(b0) { float4 c0[15]; };
cbuffer CB2 : register(b2) { float4 c2[6]; };
cbuffer VR : register(b4) { float4 vrRemap; };
Texture2D t0 : register(t0); Texture2D t1 : register(t1); Texture2D t2 : register(t2); Texture2D t3 : register(t3);
Texture2D t4 : register(t4); Texture2D t5 : register(t5); Texture2D t6 : register(t6);
SamplerState s0 : register(s0); SamplerState s1 : register(s1); SamplerState s2 : register(s2); SamplerState s3 : register(s3);
SamplerState s4 : register(s4); SamplerState s5 : register(s5); SamplerState s6 : register(s6);
struct PSIn { float4 col : COLOR0; float4 tc0 : TEXCOORD0; float4 v2 : TEXCOORD4; float4 tc6 : TEXCOORD6; float4 v4 : TEXCOORD5; bool ff : SV_IsFrontFace; };
struct PSOut { float4 o0 : SV_Target0; float4 o1 : SV_Target1; float4 o2 : SV_Target2; };
PSOut main(PSIn i) {
    const bool flag = 0.0 != c2[5].x;
    float3 p = i.v4.xyw;
    if (flag) { const float2 a = t0.Sample(s1, float2(0.0625, 0.0)).xy; const float x = a.x * (i.v4.w - a.y) + i.v4.x; p = float3(i.v4.w != 1.0 ? x : i.v4.x, i.v4.y, i.v4.w); }
    float4 r1 = float4(p.xy / p.z, p.xy / p.z);
    r1 = r1 * c2[0].xyxy + c2[0].wzwz;
    const float2 uv = r1.zw;
    const float2 ov = uv * vrRemap.xy + vrRemap.zw;
    float3 r2 = t1.Sample(s0, uv).xyz;
    float3 r3 = sqrt(r2) - r2;
    r2 = c0[5].x * r3 + r2;
    const float lum = dot(r2, float3(0.666, 0.25075, 0.08325));
    r3 = lum * c0[1].xyz - r2;
    r2 = c0[5].y * r3 + r2;
    const float scan = t2.SampleBias(s2, ov * float2(4.0, 12.0) + c0[2].xy, 0.0).x;
    r2 *= scan;
    const float3 hex = t3.SampleBias(s3, ov + ov, 0.0).xyz;
    float g = saturate(t4.SampleBias(s4, ov * float2(24.0, 18.0), 0.0).z + hex.x);
    g = g + g;
    r2 *= hex * g;
    float3 n = t5.SampleBias(s5, float2(ov.x * 4.0 + c0[12].w, ov.y * 4.0 + c0[13].x), 0.0).xyz;
    n = n * c0[13].y + 1.0;
    const float dz = c2[1].z, dw = c2[1].w;
    float4 r5;
    r5.x = 1.0 / (t1.SampleLevel(s0, float2(uv.x + 0.000587, uv.y), 0.0).w * dz - dw);
    r5.z = 1.0 / (t1.SampleLevel(s0, float2(uv.x, uv.y + 0.000695), 0.0).w * dz - dw);
    const float dc = 1.0 / (t1.SampleLevel(s0, uv, 0.0).w * dz - dw);
    r5.y = 1.0 / (t1.SampleLevel(s0, float2(uv.x - 0.000587, uv.y), 0.0).w * dz - dw);
    r5.w = 1.0 / (t1.SampleLevel(s0, float2(uv.x, uv.y - 0.000695), 0.0).w * dz - dw);
    const float k = dc / p.z;
    float qx = k * p.x, qy = k * p.z;
    if (flag) { const float2 a = t0.Sample(s1, float2(0.0625, 0.0)).xy; const float z = -a.x * (p.z * k - a.y) + qx; qx = qy != 1.0 ? z : qx; }
    const float ex = (qx / qy) * c2[0].x + c2[0].w;
    const float edge = t6.SampleBias(s6, float2(ex * vrRemap.x + vrRemap.z + c0[3].x, dc * 0.001 + c0[3].y), 0.0).y;
    const float4 m = min(r5, c0[14].wwww) - min(dc, c0[14].w);
    float e = dot(m, float4(0.25, 0.25, 0.25, 0.25)) * (c0[14].z / dc);
    e = saturate(edge * e);
    e = e * (1.0 - e);
    float3 r0 = c0[4].xyz * c0[4].w * e;
    r0 = r2 * n + r0;
    r0 += c0[0].xyz;
    r0 = r0 * c2[4].w + c2[4].xyz;
    PSOut o; o.o0 = float4(r0 * i.v2.w + i.v2.xyz, 1.0); o.o1 = 0; o.o2 = 0; return o;
}
)HLSL";
// SCANVR: Origins' evidence scanner material 13572b59fe76fadf, rebuilt from its disassembly with t2 / t3 / t4 at the shared position.
const char kScanHlsl[] = R"HLSL(
cbuffer CB0 : register(b0) { float4 c0[10]; };
cbuffer CB2 : register(b2) { float4 c2[6]; };
cbuffer VR : register(b4) { float4 vrRemap; };
Texture2D t0 : register(t0); Texture2D t1 : register(t1); Texture2D t2 : register(t2); Texture2D t3 : register(t3);
Texture2D t4 : register(t4);
SamplerState s0 : register(s0); SamplerState s1 : register(s1); SamplerState s2 : register(s2); SamplerState s3 : register(s3);
SamplerState s4 : register(s4);
struct PSIn { float4 col : COLOR0; float4 tc0 : TEXCOORD0; float4 v2 : TEXCOORD4; float4 tc6 : TEXCOORD6; float4 v4 : TEXCOORD5; bool ff : SV_IsFrontFace; };
struct PSOut { float4 o0 : SV_Target0; float4 o1 : SV_Target1; float4 o2 : SV_Target2; };
PSOut main(PSIn i) {
    const bool flag = 0.0 != c2[5].x;
    float3 p = i.v4.xyw;
    if (flag) { const float2 a = t0.Sample(s1, float2(0.0625, 0.0)).xy; const float x = a.x * (i.v4.w - a.y) + i.v4.x; p = float3(i.v4.w != 1.0 ? x : i.v4.x, i.v4.y, i.v4.w); }
    const float2 uv = (p.xy / p.z) * c2[0].xy + c2[0].wz;
    const float2 ov = uv * vrRemap.xy + vrRemap.zw; // the shared overlay position
    float3 s = t1.Sample(s0, uv).xyz;
    s = c0[2].x * (sqrt(s) - s) + s;
    float3 hex = t2.SampleBias(s2, ov + ov, 0.0).xyz;
    const float scan = t3.SampleBias(s3, ov * float2(1.0, 24.0), 0.0).z;
    float g = saturate(scan + hex.x);
    g = g + g;
    hex *= g;
    s *= hex;
    float3 n = t4.SampleBias(s4, ov * 4.0 + c0[8].zw, 0.0).xyz;
    n = n * c0[9].x + 1.0;
    const float3 r2 = n * s;
    const float w = dot(r2, float3(0.3, 0.59, 0.11));
    float3 r0 = w - s * n;
    r0 = c0[9].y * r0 + r2;
    const float w2 = dot(r0, float3(0.666, 0.25075, 0.08325));
    const float3 r1 = w2 * c0[1].xyz - r0;
    r0 = c0[9].z * r1 + r0;
    r0 += c0[0].xyz;
    r0 = r0 * c2[4].w + c2[4].xyz;
    PSOut o; o.o0 = float4(r0 * i.v2.w + i.v2.xyz, 1.0); o.o1 = 0; o.o2 = 0; return o;
}
)HLSL";
ID3D11PixelShader* g_vrPsK[2] = {};       // rebuilt detective / scanner
ID3D11Buffer* g_vrCb = nullptr;
bool g_vrTriedK[2] = {};
ID3D11PixelShader* g_gamePs = nullptr;
ID3D11Buffer* g_gameCb = nullptr;
bool g_isSwapped = false;
// City detvr_remap: this eye's screen -> its frustum tangents -> the union frustum of both eyes.
void remap(float out[4]) {
    out[0] = 1.0f; out[1] = 1.0f; out[2] = 0.0f; out[3] = 0.0f;
    const int sign = g_passEye.load(std::memory_order_relaxed);
    float a[2][4];
    if (!sign || !bvr::vr::eye_fov_angles(0, a[0]) || !bvr::vr::eye_fov_angles(1, a[1])) return;
    float t[2][4];
    for (int e = 0; e < 2; ++e) for (int k = 0; k < 4; ++k) t[e][k] = tanf(a[e][k]);
    const float L = (std::min)(t[0][0], t[1][0]), R = (std::max)(t[0][1], t[1][1]);
    const float U = (std::max)(t[0][2], t[1][2]), D = (std::min)(t[0][3], t[1][3]);
    const float* e = t[sign < 0 ? 0 : 1];
    if (R - L < 1e-4f || U - D < 1e-4f) return;
    out[0] = (e[1] - e[0]) / (R - L); out[2] = (e[0] - L) / (R - L);
    out[1] = (e[2] - e[3]) / (U - D); out[3] = (U - e[2]) / (U - D);
}
bool ready(ID3D11DeviceContext* ctx, int kind) {
    if (g_vrPsK[kind] && g_vrCb) return true;
    if (g_vrTriedK[kind]) return false;
    g_vrTriedK[kind] = true;
    ID3DBlob* code = nullptr;
    ID3DBlob* err = nullptr;
    const char* src = kind ? kScanHlsl : kDetVrHlsl;
    const size_t srcLen = kind ? sizeof(kScanHlsl) - 1 : sizeof(kDetVrHlsl) - 1;
    const HRESULT hr = D3DCompile(src, srcLen, kind ? "scanvr" : "detvr", nullptr, nullptr, "main", "ps_5_0",
                                  D3DCOMPILE_OPTIMIZATION_LEVEL3, 0, &code, &err);
    if (FAILED(hr) || !code) {
        BVR_LOG("[bo] DETVR: compile FAILED %08X %s", unsigned(hr), err ? static_cast<const char*>(err->GetBufferPointer()) : "");
        if (err) err->Release();
        if (code) code->Release();
        return false;
    }
    if (err) err->Release();
    ID3D11Device* dev = nullptr;
    ctx->GetDevice(&dev);
    if (dev) {
        dev->CreatePixelShader(code->GetBufferPointer(), code->GetBufferSize(), nullptr, &g_vrPsK[kind]);
        if (!g_vrCb) {
            D3D11_BUFFER_DESC bd{};
            bd.ByteWidth = 16; bd.Usage = D3D11_USAGE_DEFAULT; bd.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
            dev->CreateBuffer(&bd, nullptr, &g_vrCb);
        }
        dev->Release();
    }
    code->Release();
    BVR_LOG("[bo] DETVR: Origins' %s material rebuilt %s", kind ? "evidence scanner" : "detective",
            g_vrPsK[kind] && g_vrCb ? "and ready" : "but NOT created");
    return g_vrPsK[kind] && g_vrCb;
}
} // namespace

void install() {
    ID3D11Device* dev = nullptr;
    const D3D_FEATURE_LEVEL fl = D3D_FEATURE_LEVEL_11_0;
    if (FAILED(D3D11CreateDevice(nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr, 0, &fl, 1, D3D11_SDK_VERSION, &dev, nullptr, nullptr)) || !dev) {
        BVR_LOG("[bo] DETVR: no throwaway device - detective shader not recorded");
        return;
    }
    void* target = (*reinterpret_cast<void***>(dev))[15]; // ID3D11Device::CreatePixelShader
    dev->Release();
    if (MH_CreateHook(target, reinterpret_cast<void*>(&create_ps_detour), reinterpret_cast<void**>(&g_createPsOrig)) != MH_OK ||
        MH_QueueEnableHook(target) != MH_OK || MH_ApplyQueued() != MH_OK) {
        BVR_LOG("[bo] DETVR: CreatePixelShader hook FAILED");
        return;
    }
    BVR_LOG("[bo] DETVR: CreatePixelShader hooked (%p) - detective shader candidates are recorded", target);
}

bool before_draw(ID3D11DeviceContext* ctx, unsigned vertexCount) {
    if (vertexCount != 3) return false;
    ID3D11PixelShader* ps = nullptr;
    ctx->PSGetShader(&ps, nullptr, nullptr);
    if (!ps) return false;
    const int n = g_candN.load(std::memory_order_acquire);
    if (!n) { ps->Release(); return false; }
    int hit = -1;
    for (int i = 0; i < n && hit < 0; ++i) if (g_cand[i].ps == ps) hit = i;
    if (hit < 0) { ps->Release(); return false; }
    if (g_cand[hit].draws.fetch_add(1, std::memory_order_relaxed) == 0)
        BVR_LOG("[bo] DETVR: %s drawn (eye %d) - overlay %s", g_cand[hit].kind ? "evidence scanner" : "detective mode",
                g_passEye.load(), g_tryCity.load() ? "fused for both eyes" : "as the game draws it");
    const int kind = g_cand[hit].kind;
    if (kind == 1) g_scanDrawMs.store(GetTickCount64(), std::memory_order_relaxed); // SCANHUD: the scanner is up
    if (!g_tryCity.load(std::memory_order_relaxed) || !ready(ctx, kind)) { ps->Release(); return false; }
    float r[4];
    remap(r);
    ctx->UpdateSubresource(g_vrCb, 0, nullptr, r, 0, 0);
    g_gamePs = ps; // reference kept until after_draw
    ctx->PSGetConstantBuffers(4, 1, &g_gameCb);
    ctx->PSSetShader(g_vrPsK[kind], nullptr, 0);
    ctx->PSSetConstantBuffers(4, 1, &g_vrCb);
    g_isSwapped = true;
    g_swapped.fetch_add(1, std::memory_order_relaxed);
    return false;
}

void after_draw(ID3D11DeviceContext* ctx) {
    if (!g_isSwapped) return;
    g_isSwapped = false;
    ctx->PSSetShader(g_gamePs, nullptr, 0);
    ctx->PSSetConstantBuffers(4, 1, &g_gameCb);
    if (g_gamePs) { g_gamePs->Release(); g_gamePs = nullptr; }
    if (g_gameCb) { g_gameCb->Release(); g_gameCb = nullptr; }
}

void set_render_pass(int eyeSign) { g_passEye.store(eyeSign, std::memory_order_relaxed); }
bool scanner_active() { // SCANHUD
    const ULONGLONG t = g_scanDrawMs.load(std::memory_order_relaxed);
    return t && GetTickCount64() - t < 300;
}

bool enabled() { return g_tryCity.load(); }
void set_enabled(bool on) { g_tryCity.store(on); }
bool draw_ui() {
    bool tc = g_tryCity.load();
    const bool changed = ImGui::Checkbox("detective and evidence scanner overlays fused for both eyes (one vignette)", &tc);
    if (changed) {
        g_tryCity.store(tc);
        BVR_LOG("[bo] DETVR: detective overlay %s (rebuilt draws so far %u)", tc ? "fused" : "as the game draws it", g_swapped.load());
    }
    if (bvr::overlay::show_tests())
        ImGui::TextDisabled("   detective material found %d, rebuilt draws %u", g_candN.load(), g_swapped.load());
    return changed;
}
} // namespace bvr::bo::detective
