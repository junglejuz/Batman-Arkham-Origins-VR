// FLAREROLL - see flare_roll.h. Evidence (STATIC, framedump_044343 2026-10-11, lens flares on): after the scene, the game halves
// the picture (764x820), quarters it (384x412 R16G16B16A16_UNORM, copied), draws three identical full-screen passes into a 382x410
// R16G16B16A16_FLOAT target (each copied back and read again = an iterative streak), a fourth that adds another quarter picture,
// two 2D bloom passes, then the tone-map step reads the scene + both quarter results + the colour table. Rotating an isotropic
// bloom in and out is harmless, so every such quarter-size step is wrapped the same way.
#include "flare_roll.h"
#include "core/gfx/frame_inspector.h"
#include "core/ui/overlay.h"
#include "core/util/log.h"
#include <windows.h>
#include <d3d11.h>
#include <d3dcompiler.h>
#include <imgui.h>
#include <atomic>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <string>
#include <unordered_map>
#include <MinHook.h>

namespace bvr::bo::flare_roll {
namespace {
std::atomic<bool> g_on{true};
// LEVELWORLD (JJ 2026-10-27 night: "When I turn my head left, the streak rotates clockwise ... right, anticlockwise"): with the stick
// pitching the game camera ~9 deg, a sideways look tilts the whole game world ~6 deg in the view (computed: +9 deg pitch, 40 deg left
// = +5.8 deg clockwise); the streak must lie level with that world, not with the real room. true = the game world's horizon at the
// picture centre, false = the headset's own roll (HEADROLL). Panel switch to compare.
std::atomic<bool> g_levelWorld{false}; // ALWAYSALIGN: the real room by default (JJ 19:47: the game world "only a couple of degrees")
// The quarter-size chain turn (below) found nothing to turn back in this game's current frame (LIVE 18:05) - kept, switched off.
std::atomic<bool> g_chain{false};
std::atomic<float> g_angle[2] = {};         // per eye (0 left, 1 right), picture pixels, y down
std::atomic<unsigned> g_passes{0}, g_rotIn{0}, g_rotOut{0}, g_failed{0}, g_skippedDepth{0};
std::atomic<float> g_lastDeg{0};
constexpr float kMinAngle = 0.0035f;        // 0.2 deg: below this nothing is touched

const char kHlsl[] = R"(
cbuffer P : register(b0) { float4 rot; float4 size; }; // rot = (cos, sin); size = (w, h, 1 / w, 1 / h)
Texture2D S0 : register(t0);
SamplerState L : register(s0);
float4 vs_main(uint id : SV_VertexID) : SV_Position {
    float2 t = float2((id << 1) & 2, id & 2);
    return float4(t * float2(2, -2) + float2(-1, 1), 0, 1);
}
// out(p) = in(c + R (p - c)), pixel space (keeps the picture's own aspect); outside the picture = black
float4 ps_main(float4 p : SV_Position) : SV_Target {
    float2 c = size.xy * 0.5, d = p.xy - c;
    float2 q = c + float2(rot.x * d.x - rot.y * d.y, rot.y * d.x + rot.x * d.y);
    float2 uv = q * size.zw;
    if (any(uv < 0) || any(uv > 1)) return 0;
    float4 v = S0.SampleLevel(L, uv, 0);
    return (v == v) ? v : 0;
}
)";
struct Cb { float rot[4], size[4]; };
ID3D11VertexShader* g_vs = nullptr;
ID3D11PixelShader* g_ps = nullptr;
ID3D11Buffer* g_cb = nullptr;
ID3D11SamplerState* g_samp = nullptr;
ID3D11BlendState* g_opaque = nullptr;
ID3D11RasterizerState* g_rs = nullptr;
ID3D11DepthStencilState* g_ds = nullptr;
bool g_pipeFailed = false;

bool ensure_pipeline(ID3D11Device* dev) {
    if (g_vs) return true;
    if (g_pipeFailed) return false;
    auto compile = [&](const char* entry, const char* target, ID3DBlob** out) {
        ID3DBlob* err = nullptr;
        const HRESULT hr = D3DCompile(kHlsl, sizeof(kHlsl) - 1, "bo_flareroll", nullptr, nullptr, entry, target, 0, 0, out, &err);
        if (err) { BVR_LOG("[flareroll] shader %s: %s", entry, static_cast<const char*>(err->GetBufferPointer())); err->Release(); }
        return SUCCEEDED(hr) && *out;
    };
    ID3DBlob* b = nullptr;
    bool ok = compile("vs_main", "vs_4_0", &b) && SUCCEEDED(dev->CreateVertexShader(b->GetBufferPointer(), b->GetBufferSize(), nullptr, &g_vs));
    if (b) { b->Release(); b = nullptr; }
    ok = ok && compile("ps_main", "ps_4_0", &b) && SUCCEEDED(dev->CreatePixelShader(b->GetBufferPointer(), b->GetBufferSize(), nullptr, &g_ps));
    if (b) b->Release();
    if (ok) {
        D3D11_BUFFER_DESC cd{sizeof(Cb), D3D11_USAGE_DEFAULT, D3D11_BIND_CONSTANT_BUFFER, 0, 0, 0};
        D3D11_SAMPLER_DESC sd{};
        sd.Filter = D3D11_FILTER_MIN_MAG_MIP_LINEAR;
        sd.AddressU = sd.AddressV = sd.AddressW = D3D11_TEXTURE_ADDRESS_CLAMP;
        sd.MaxLOD = D3D11_FLOAT32_MAX;
        D3D11_BLEND_DESC od{};
        od.RenderTarget[0].RenderTargetWriteMask = D3D11_COLOR_WRITE_ENABLE_ALL;
        D3D11_RASTERIZER_DESC rd{};
        rd.FillMode = D3D11_FILL_SOLID; rd.CullMode = D3D11_CULL_NONE; rd.DepthClipEnable = TRUE;
        D3D11_DEPTH_STENCIL_DESC dd{};
        ok = SUCCEEDED(dev->CreateBuffer(&cd, nullptr, &g_cb)) && SUCCEEDED(dev->CreateSamplerState(&sd, &g_samp)) &&
             SUCCEEDED(dev->CreateBlendState(&od, &g_opaque)) && SUCCEEDED(dev->CreateRasterizerState(&rd, &g_rs)) &&
             SUCCEEDED(dev->CreateDepthStencilState(&dd, &g_ds));
    }
    g_pipeFailed = !ok;
    BVR_LOG("[flareroll] pipeline %s", ok ? "ready" : "setup FAILED - lens flares stay as the game draws them");
    return ok;
}

// Our turned copies: a small pool per picture size, used round-robin (the immediate context keeps the order of our writes and the
// game's reads, so a copy may be rewritten once the draw that reads it has been issued).
struct Tex {
    ID3D11Texture2D* t = nullptr; ID3D11RenderTargetView* rtv = nullptr; ID3D11ShaderResourceView* srv = nullptr; UINT w = 0, h = 0;
};
constexpr int kPool = 24;
Tex g_pool[kPool];
int g_poolNext = 0;
Tex* get_tex(ID3D11Device* dev, UINT w, UINT h) {
    for (int k = 0; k < kPool; ++k) { // the next free-standing slot of this size, else a new one
        Tex& t = g_pool[(g_poolNext + k) % kPool];
        if (t.t && t.w == w && t.h == h) { g_poolNext = (g_poolNext + k + 1) % kPool; return &t; }
    }
    Tex& t = g_pool[g_poolNext];
    g_poolNext = (g_poolNext + 1) % kPool;
    if (t.srv) t.srv->Release(); if (t.rtv) t.rtv->Release(); if (t.t) t.t->Release();
    t = Tex{};
    D3D11_TEXTURE2D_DESC d{};
    d.Width = w; d.Height = h; d.MipLevels = 1; d.ArraySize = 1; d.Format = DXGI_FORMAT_R16G16B16A16_FLOAT;
    d.SampleDesc.Count = 1; d.Usage = D3D11_USAGE_DEFAULT; d.BindFlags = D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE;
    if (FAILED(dev->CreateTexture2D(&d, nullptr, &t.t)) || FAILED(dev->CreateRenderTargetView(t.t, nullptr, &t.rtv)) ||
        FAILED(dev->CreateShaderResourceView(t.t, nullptr, &t.srv))) {
        if (t.srv) t.srv->Release(); if (t.rtv) t.rtv->Release(); if (t.t) t.t->Release();
        t = Tex{};
        return nullptr;
    }
    t.w = w; t.h = h;
    return &t;
}
bool tex_desc(ID3D11Resource* r, D3D11_TEXTURE2D_DESC* d) {
    if (!r) return false;
    D3D11_RESOURCE_DIMENSION dim{};
    r->GetType(&dim);
    if (dim != D3D11_RESOURCE_DIMENSION_TEXTURE2D) return false;
    static_cast<ID3D11Texture2D*>(r)->GetDesc(d);
    return true;
}
bool rgba16(DXGI_FORMAT f) {
    return f == DXGI_FORMAT_R16G16B16A16_FLOAT || f == DXGI_FORMAT_R16G16B16A16_UNORM || f == DXGI_FORMAT_R16G16B16A16_TYPELESS;
}

// This eye pass: the pictures that already hold LEVEL content (raw identity only, never dereferenced after the pass).
constexpr int kLevelMax = 32;
ID3D11Resource* g_level[kLevelMax] = {};
int g_levelN = 0;
float g_eyeAngle = 0;          // this eye pass
bool g_active = false;
// ALWAYSALIGN (JJ 2026-10-27 night after the 19:47 run: "It only improves it by a couple of degrees, it's still rotating"): the flare
// pieces were only touched while the eye's tilt was over 0.2 deg - with "level with the real room" and the head level that is never, so
// TEXALIGN never undid the GAME's own turn (19:40). The pieces are now aligned in every eye picture.
bool g_spritePass = false;
std::atomic<bool> g_hideTest{false}; // TESTS: hide the pieces we align (is the streak one of them?)
std::atomic<unsigned> g_hidden{0};
int g_census = 0, g_censusLines = 0;   // CENSUS: 0 waiting, 1 listing this eye picture, 2 done
ID3D11DepthStencilView* g_heldDsv = nullptr; // DEPTHOFF: the game's depth buffer, put back after its draw
const char* fmt_name(DXGI_FORMAT f) {
    switch (f) {
    case DXGI_FORMAT_R16G16B16A16_FLOAT: return "rgba16f"; case DXGI_FORMAT_R16G16B16A16_UNORM: return "rgba16";
    case DXGI_FORMAT_R8G8B8A8_UNORM: return "rgba8"; case DXGI_FORMAT_R8G8B8A8_UNORM_SRGB: return "rgba8s";
    case DXGI_FORMAT_R11G11B10_FLOAT: return "r11g11b10"; case DXGI_FORMAT_R32_FLOAT: return "r32f"; case DXGI_FORMAT_R8_UNORM: return "r8";
    case DXGI_FORMAT_B8G8R8A8_UNORM: return "bgra8"; case DXGI_FORMAT_R24G8_TYPELESS: return "r24g8"; default: return "other";
    }
}
UINT g_eyeW = 0;               // widest full-screen post target seen (the eye picture's width)
bool is_level(ID3D11Resource* r) { for (int i = 0; i < g_levelN; ++i) if (g_level[i] == r) return true; return false; }
void set_level(ID3D11Resource* r, bool on) {
    for (int i = 0; i < g_levelN; ++i)
        if (g_level[i] == r) { if (!on) g_level[i] = g_level[--g_levelN]; return; }
    if (on && g_levelN < kLevelMax) g_level[g_levelN++] = r;
}

// One turned copy of `src` (same size): out(p) = in(c + R(angle) (p - c)). The caller's state is saved / restored around it.
ID3D11ShaderResourceView* turned(ID3D11DeviceContext* ctx, ID3D11ShaderResourceView* src, const D3D11_TEXTURE2D_DESC& d, float angle) {
    ID3D11Device* dev = nullptr;
    ctx->GetDevice(&dev);
    Tex* t = dev && ensure_pipeline(dev) ? get_tex(dev, d.Width, d.Height) : nullptr;
    if (dev) dev->Release();
    if (!t) { g_failed.fetch_add(1, std::memory_order_relaxed); return nullptr; }
    Cb cb{};
    cb.rot[0] = cosf(angle); cb.rot[1] = sinf(angle);
    cb.size[0] = float(d.Width); cb.size[1] = float(d.Height); cb.size[2] = 1.0f / float(d.Width); cb.size[3] = 1.0f / float(d.Height);
    ID3D11ShaderResourceView* none = nullptr;
    ctx->PSSetShaderResources(0, 1, &none);
    ctx->OMSetRenderTargets(1, &t->rtv, nullptr);
    D3D11_VIEWPORT vp{0, 0, float(d.Width), float(d.Height), 0, 1};
    ctx->RSSetViewports(1, &vp);
    ctx->UpdateSubresource(g_cb, 0, nullptr, &cb, 0, 0);
    ctx->PSSetShaderResources(0, 1, &src);
    ctx->Draw(3, 0);
    ctx->PSSetShaderResources(0, 1, &none);
    return t->srv;
}

// The game's state our turning passes touch (they run before the game's draw, which then sees its own state back).
struct Saved {
    ID3D11RenderTargetView* rtv[8] = {}; ID3D11DepthStencilView* dsv = nullptr; ID3D11BlendState* blend = nullptr; FLOAT bf[4] = {};
    UINT mask = 0; ID3D11DepthStencilState* depth = nullptr; UINT ref = 0; ID3D11RasterizerState* rs = nullptr; UINT nvp = 1;
    D3D11_VIEWPORT vp{}; ID3D11VertexShader* vs = nullptr; ID3D11HullShader* hs = nullptr; ID3D11DomainShader* ds = nullptr;
    ID3D11GeometryShader* gs = nullptr; ID3D11PixelShader* ps = nullptr; ID3D11Buffer* pcb = nullptr; ID3D11SamplerState* samp = nullptr;
    ID3D11InputLayout* il = nullptr; D3D11_PRIMITIVE_TOPOLOGY topo{};
    void save(ID3D11DeviceContext* c) {
        c->OMGetRenderTargets(8, rtv, &dsv); c->OMGetBlendState(&blend, bf, &mask); c->OMGetDepthStencilState(&depth, &ref);
        c->RSGetState(&rs); c->RSGetViewports(&nvp, &vp); c->VSGetShader(&vs, nullptr, nullptr); c->HSGetShader(&hs, nullptr, nullptr);
        c->DSGetShader(&ds, nullptr, nullptr); c->GSGetShader(&gs, nullptr, nullptr); c->PSGetShader(&ps, nullptr, nullptr);
        c->PSGetConstantBuffers(0, 1, &pcb); c->PSGetSamplers(0, 1, &samp); c->IAGetInputLayout(&il); c->IAGetPrimitiveTopology(&topo);
    }
    void ours(ID3D11DeviceContext* c) {
        c->OMSetDepthStencilState(g_ds, 0); c->RSSetState(g_rs); c->OMSetBlendState(g_opaque, nullptr, 0xFFFFFFFF);
        c->IASetInputLayout(nullptr); c->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
        c->VSSetShader(g_vs, nullptr, 0); c->HSSetShader(nullptr, nullptr, 0); c->DSSetShader(nullptr, nullptr, 0);
        c->GSSetShader(nullptr, nullptr, 0); c->PSSetShader(g_ps, nullptr, 0); c->PSSetConstantBuffers(0, 1, &g_cb);
        c->PSSetSamplers(0, 1, &g_samp);
    }
    void restore(ID3D11DeviceContext* c) {
        c->OMSetRenderTargets(8, rtv, dsv); c->OMSetBlendState(blend, bf, mask); c->OMSetDepthStencilState(depth, ref);
        c->RSSetState(rs); if (nvp) c->RSSetViewports(1, &vp); c->VSSetShader(vs, nullptr, 0); c->HSSetShader(hs, nullptr, 0);
        c->DSSetShader(ds, nullptr, 0); c->GSSetShader(gs, nullptr, 0); c->PSSetShader(ps, nullptr, 0);
        c->PSSetConstantBuffers(0, 1, &pcb); c->PSSetSamplers(0, 1, &samp); c->IASetInputLayout(il); c->IASetPrimitiveTopology(topo);
        IUnknown* all[] = {dsv, blend, depth, rs, vs, hs, ds, gs, ps, pcb, samp, il};
        for (IUnknown* u : all) if (u) u->Release();
        for (auto* r : rtv) if (r) r->Release();
    }
};
// FLARESPRITE (JJ 2026-10-27 night: the anamorphic streak "still rotating along its z-axis when I roll my head"; LIVE 18:05 census:
// in this game's current frame there is NO quarter-size streak chain - one quarter step and the tone map, nothing turned back).
// framedump_044343: the lens-flare pieces are single-quad Draw(4) strips laid over the HDR scene (five per frame, each its own
// flare texture, the translucency caller 0x630DB2) - the streak is one of them, stretched along the picture's x axis. Each such
// quad is turned about its own centre by the eye's tilt, so it lies along the world's horizon. Done in its own draw by a geometry
// shader built from the draw's vertex shader outputs (AKVR hudsplit.cpp MKGS method: read the VS output signature at creation).
// A triangle of a quad holds the quad's diagonal as its longest edge, so both triangles find the same centre; full-screen quads
// (every corner on the picture's edge) are left alone.
std::atomic<bool> g_sprites{true};
std::atomic<bool> g_faceGame{true}; // FACEGAME (TESTS switch)
std::atomic<unsigned> g_faced{0};
SRWLOCK g_faceLock = SRWLOCK_INIT;
float g_eyeFace[2][10] = {}; bool g_eyeFaceOk[2] = {};  // game thread: lens P00, P11, P20, P21, game right (3), game up (3) per eye
float g_passFace[10] = {}; bool g_passFaceOk = false;   // render thread: this eye picture's
std::atomic<unsigned> g_spriteDraws{0}, g_spriteNoGs{0}, g_gsBuilt{0}, g_gsFailed{0}, g_vsNoted{0};
struct GsEnt { std::string src; ID3D11GeometryShader* gs = nullptr; int state = 0; bool logged = false; }; // 0 not built, 1 built, -1 failed / unsupported
std::unordered_map<void*, GsEnt> g_gsMap;
SRWLOCK g_gsLock = SRWLOCK_INIT;
char g_gsErr[256] = {};
std::string gs_source(const uint8_t* c, size_t n) {
    if (n < 32 || memcmp(c, "DXBC", 4) != 0) return "";
    const uint32_t nch = *reinterpret_cast<const uint32_t*>(c + 28);
    for (uint32_t k = 0; k < nch && 32 + k * 4 + 4 <= n; ++k) {
        const uint32_t off = *reinterpret_cast<const uint32_t*>(c + 32 + k * 4);
        if (off + 8 > n) return "";
        if (memcmp(c + off, "OSGN", 4) != 0) continue;
        const uint8_t* d = c + off + 8;
        const uint32_t dsz = *reinterpret_cast<const uint32_t*>(c + off + 4);
        if (off + 8 + dsz > n || dsz < 8) return "";
        const uint32_t cnt = *reinterpret_cast<const uint32_t*>(d);
        std::string st = "struct V {\n";
        std::string uvs[8]; int nUv = 0;
        int lastReg = -1; bool havePos = false;
        for (uint32_t i = 0; i < cnt; ++i) {
            const uint8_t* e = d + 8 + i * 24;
            if (e + 24 > d + dsz) return "";
            const uint32_t nameOff = *reinterpret_cast<const uint32_t*>(e), semIdx = *reinterpret_cast<const uint32_t*>(e + 4),
                           sysv = *reinterpret_cast<const uint32_t*>(e + 8), ctype = *reinterpret_cast<const uint32_t*>(e + 12),
                           reg = *reinterpret_cast<const uint32_t*>(e + 16);
            const uint8_t mask = e[20];
            if (nameOff >= dsz || int(reg) <= lastReg) return ""; // shared registers: not handled
            lastReg = int(reg);
            int comps = 0;
            for (int b = 0; b < 4; ++b) if (mask & (1 << b)) ++comps;
            if (comps == 0 || mask != ((1 << comps) - 1)) return "";
            const char* ty = ctype == 1 ? "uint" : ctype == 2 ? "int" : "float";
            char line[160];
            if (sysv == 1) { if (comps != 4) return ""; snprintf(line, sizeof line, "  float4 pos : SV_Position;\n"); havePos = true; }
            else if (sysv != 0) return ""; // clip / cull distances etc.: not handled
            else snprintf(line, sizeof line, "  %s%s f%u : %s%u;\n", ty, comps == 1 ? "" : comps == 2 ? "2" : comps == 3 ? "3" : "4", i,
                          reinterpret_cast<const char*>(d) + nameOff, semIdx);
            st += line;
            // TEXALIGN2: every float pair of every TEXCOORD output is a candidate texture coordinate (LIVE 19:40: the first one alone
            // changed nothing - most likely a colour or other constant-per-piece value)
            if (sysv == 0 && ctype == 3 && comps >= 2 && !_stricmp(reinterpret_cast<const char*>(d) + nameOff, "TEXCOORD")) {
                char f[16];
                if (nUv < 8) { snprintf(f, sizeof f, "f%u.xy", i); uvs[nUv++] = f; }
                if (comps == 4 && nUv < 8) { snprintf(f, sizeof f, "f%u.zw", i); uvs[nUv++] = f; }
            }
        }
        if (!havePos) return "";
        st += "};\n";
        // TEXALIGN (JJ 2026-10-27 night: the streak "no longer rotating when I roll my head ... but it is rotating when I look left and
        // right"; with the head level our turn is zero, so the GAME turns its pieces as the light crosses the picture): the piece's
        // texture x axis on the picture (d position / d u, from its three corners) is turned onto the head-level direction (mod 180 deg:
        // a streak has no front), so the piece is drawn as authored, level, wherever it is; without a usable texture coordinate the
        // head-roll turn alone (as before).
        // The first candidate that behaves like a texture coordinate over this triangle (a real spread, not constant, not screen-size
        // tiny) gives the piece's texture x axis.
        std::string align = "  float ang = atan2(R.y, R.x); bool got = false;\n  float2 pa = p[1] - p[0], pb = p[2] - p[0];\n";
        for (int k = 0; k < nUv; ++k)
            align += "  if (!got) { float2 ua = i[1]." + uvs[k] + " - i[0]." + uvs[k] + ", ub = i[2]." + uvs[k] + " - i[0]." + uvs[k] + ";\n"
                     "    float det = ua.x * ub.y - ua.y * ub.x;\n"
                     "    if (abs(det) > 0.02 && abs(det) < 50) { float2 du = (pa * ub.y - pb * ua.y) / det;\n"
                     "      if (dot(du, du) > 1e-4) { float t = ang - atan2(du.y, du.x); ang = t - 3.14159265 * round(t / 3.14159265); got = true; } } }\n";
        // FACEGAME (JJ 2026-10-27 night: "I can guarantee it is rotating around its y-axis because when there's a wall there, the flare
        // streak disappears into the wall when it rotates"): the streak cards stand in the world facing the HEAD-TRACKED view. Mode 1
        // rebuilds each card facing the GAME camera instead: its corners back into this eye's view space (the eye's own lens), each
        // corner's offset from the card centre along the eye's right / up is laid along the game camera's right / up (given in this
        // eye's view space), projected again; depth stays each corner's own (no new cuts into walls).
        return "cbuffer BoFlare : register(b0) { float4 R; float4 P0; float4 P1; float4 GR; float4 GU; };\n"
               "// R = cos, sin (head roll), viewport half width / height; P0 = lens P00, P11, P20, P21; P1.x = mode; GR / GU = game camera right / up\n" + st +
               "[maxvertexcount(3)] void main(triangle V i[3], inout TriangleStream<V> s) {\n"
               "  if (P1.x > 0.5) {\n"
               "    float3 v[3]; bool ok = true;\n"
               "    [unroll] for (int k = 0; k < 3; ++k) { float4 q = i[k].pos; ok = ok && q.w > 1.0;"
               " v[k] = float3((q.x - q.w * P0.z) / P0.x, (q.y - q.w * P0.w) / P0.y, q.w); }\n"
               "    float e01 = dot(v[0] - v[1], v[0] - v[1]), e12 = dot(v[1] - v[2], v[1] - v[2]), e20 = dot(v[2] - v[0], v[2] - v[0]);\n"
               "    float span = sqrt(max(e01, max(e12, e20)));\n"
               "    ok = ok && abs(v[1].z - v[0].z) <= 0.05 * span + 0.5 && abs(v[2].z - v[0].z) <= 0.05 * span + 0.5; // flat to the view\n"
               "    float3 m = (e01 >= e12 && e01 >= e20) ? (v[0] + v[1]) * 0.5 : (e12 >= e20 ? (v[1] + v[2]) * 0.5 : (v[2] + v[0]) * 0.5);\n"
               "    [unroll] for (int j = 0; j < 3; ++j) { V o = i[j];\n"
               "      if (ok) { float3 d = v[j] - m; float3 n = m + d.x * GR.xyz + d.y * GU.xyz; float ndcz = o.pos.z / o.pos.w;\n"
               "        o.pos = float4(n.x * P0.x + n.z * P0.z, n.y * P0.y + n.z * P0.w, ndcz * n.z, n.z); }\n"
               "      s.Append(o); }\n"
               "    s.RestartStrip(); return;\n"
               "  }\n"
               "  float2 p[3]; bool full = true;\n"
               "  [unroll] for (int k = 0; k < 3; ++k) { float2 q = i[k].pos.xy / i[k].pos.w; full = full && all(abs(q) > 0.97);"
               " p[k] = float2(q.x * R.z, -q.y * R.w); }\n"
               "  float d01 = dot(p[0] - p[1], p[0] - p[1]), d12 = dot(p[1] - p[2], p[1] - p[2]), d20 = dot(p[2] - p[0], p[2] - p[0]);\n"
               "  float2 c = (d01 >= d12 && d01 >= d20) ? (p[0] + p[1]) * 0.5 : (d12 >= d20 ? (p[1] + p[2]) * 0.5 : (p[2] + p[0]) * 0.5);\n" +
               align +
               "  float cs = cos(ang), sn = sin(ang);\n"
               "  [unroll] for (int j = 0; j < 3; ++j) { V o = i[j];\n"
               "    if (!full) { float2 d = p[j] - c; float2 q = c + float2(cs * d.x - sn * d.y, sn * d.x + cs * d.y);"
               " o.pos.xy = float2(q.x / R.z, -q.y / R.w) * o.pos.w; }\n"
               "    s.Append(o); }\n"
               "  s.RestartStrip();\n}\n";
    }
    return "";
}
using CreateVSFn = HRESULT(STDMETHODCALLTYPE*)(ID3D11Device*, const void*, SIZE_T, ID3D11ClassLinkage*, ID3D11VertexShader**);
CreateVSFn g_origCreateVS = nullptr;
HRESULT STDMETHODCALLTYPE create_vs_detour(ID3D11Device* dev, const void* code, SIZE_T len, ID3D11ClassLinkage* cl, ID3D11VertexShader** out) {
    const HRESULT hr = g_origCreateVS(dev, code, len, cl, out);
    if (SUCCEEDED(hr) && out && *out && code && len >= 32) {
        std::string src = gs_source(static_cast<const uint8_t*>(code), len);
        AcquireSRWLockExclusive(&g_gsLock);
        GsEnt& e = g_gsMap[*out]; // a reused object address replaces the old entry
        if (e.gs) { e.gs->Release(); e.gs = nullptr; }
        e.src = std::move(src);
        e.state = e.src.empty() ? -1 : 0;
        ReleaseSRWLockExclusive(&g_gsLock);
        g_vsNoted.fetch_add(1, std::memory_order_relaxed);
    }
    return hr;
}
ID3D11GeometryShader* gs_for(ID3D11VertexShader* vs) {
    if (!vs) return nullptr;
    ID3D11GeometryShader* out = nullptr;
    AcquireSRWLockExclusive(&g_gsLock);
    auto it = g_gsMap.find(vs);
    GsEnt* e = it != g_gsMap.end() ? &it->second : nullptr;
    if (e && e->state == 0) {
        e->state = -1;
        ID3DBlob* b = nullptr; ID3DBlob* er = nullptr; ID3D11Device* dev = nullptr;
        vs->GetDevice(&dev);
        if (dev && SUCCEEDED(D3DCompile(e->src.data(), e->src.size(), "bo_flaresprite", nullptr, nullptr, "main", "gs_4_0", 0, 0, &b, &er)) && b &&
            SUCCEEDED(dev->CreateGeometryShader(b->GetBufferPointer(), b->GetBufferSize(), nullptr, &e->gs))) {
            e->state = 1; g_gsBuilt.fetch_add(1, std::memory_order_relaxed);
        } else {
            g_gsFailed.fetch_add(1, std::memory_order_relaxed);
            if (!g_gsErr[0]) snprintf(g_gsErr, sizeof g_gsErr, "%s", er ? static_cast<const char*>(er->GetBufferPointer()) : "CreateGeometryShader failed");
        }
        if (b) b->Release(); if (er) er->Release(); if (dev) dev->Release();
    }
    if (e && e->state == 1) out = e->gs;
    if (e && !e->logged) { // the flare piece's vertex shader outputs, once each (which one is its texture coordinate)
        e->logged = true;
        std::string line;
        for (size_t a = e->src.find("struct V {"); a != std::string::npos && a < e->src.size(); ) {
            const size_t b = e->src.find('\n', a);
            if (b == std::string::npos) break;
            line += e->src.substr(a, b - a) + " ";
            if (e->src.compare(a, 2, "};") == 0) break;
            a = b + 1;
        }
        BVR_LOG("[flareroll] flare piece shader %p outputs: %s", static_cast<void*>(vs), line.c_str());
    }
    ReleaseSRWLockExclusive(&g_gsLock);
    return out;
}
ID3D11Buffer* g_gsCb = nullptr;
bool g_gsBound = false;
ID3D11GeometryShader* g_prevGs = nullptr;
ID3D11Buffer* g_prevGsCb = nullptr;
unsigned g_passCount = 0;
// PIECECENSUS (JJ 2026-10-27 night after ALWAYSALIGN: "it's still happening"; the 64x64 sprites we align are evidently lamp glows,
// not the streak): every distinct ADDITIVE draw onto the eye-size HDR scene (Draw or DrawIndexed, up to 3000 vertices / indices) is
// logged once - count, shape, its first two pictures, blend, vertex shader, and whether it is aligned - so the streak's own draw shows
// itself (a wide, thin texture). Single quads drawn indexed (6 indices, triangle list) or as a 6-vertex list are aligned too.
uint64_t g_pieceKeys[64] = {};
int g_pieceKeyN = 0;
std::atomic<unsigned> g_indexedAligned{0};
// 0 = not a piece, 1 = aligned (our GS bound, after_draw unbinds it), 2 = drop (TESTS hide switch)
int piece(ID3D11DeviceContext* ctx, unsigned count, bool indexed) {
    if (count > (indexed ? 3000u : 1000u) || !g_eyeW) return 0;
    ID3D11BlendState* bs = nullptr; FLOAT bf[4]; UINT mask = 0;
    ctx->OMGetBlendState(&bs, bf, &mask); // cheapest test first: most draws are opaque
    D3D11_BLEND_DESC bd{};
    if (bs) { bs->GetDesc(&bd); bs->Release(); }
    const auto& b0 = bd.RenderTarget[0];
    if (!bs || !b0.BlendEnable || b0.DestBlend != D3D11_BLEND_ONE) return 0; // only light ADDED onto the scene
    ID3D11RenderTargetView* rtv = nullptr;
    ctx->OMGetRenderTargets(1, &rtv, nullptr);
    if (!rtv) return 0;
    ID3D11Resource* rt = nullptr; rtv->GetResource(&rt); rtv->Release();
    D3D11_TEXTURE2D_DESC rd{};
    const bool rtOk = tex_desc(rt, &rd);
    if (rt) rt->Release();
    if (!rtOk || rd.Width * 2 < g_eyeW || !(rgba16(rd.Format) || rd.Format == DXGI_FORMAT_R11G11B10_FLOAT)) return 0;
    D3D11_PRIMITIVE_TOPOLOGY topo{};
    ctx->IAGetPrimitiveTopology(&topo);
    // STREAKCARDS (LIVE 19:55 census: the only thin textures among the additive draws are glow draw 63 - 512x128, 84 indices = 14
    // cards - and glow draw 1 - 64x256, 4 cards): batches of cards (triangle list, a multiple of 6, up to 200 cards) whose first
    // picture is at least 3:1 are aligned card by card too; meshes with square / 4x4 pictures (signs, lit surfaces) never are.
    bool thin = false, textured = false;
    {
        ID3D11ShaderResourceView* s0 = nullptr;
        ctx->PSGetShaderResources(0, 1, &s0);
        ID3D11Resource* r = nullptr;
        if (s0) { s0->GetResource(&r); s0->Release(); }
        D3D11_TEXTURE2D_DESC t0{};
        if (r && tex_desc(r, &t0)) {
            const UINT lo = t0.Width < t0.Height ? t0.Width : t0.Height, hi = t0.Width < t0.Height ? t0.Height : t0.Width;
            thin = lo >= 16 && hi >= 3 * lo;
            textured = lo >= 16; // FLATCARDS: a real picture (the lit meshes carry a 4x4 one)
        }
        if (r) r->Release();
    }
    const bool quad = (!indexed && count == 4 && topo == D3D11_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP) ||
                      (count == 6 && topo == D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST) ||
                      (thin && topo == D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST && count % 6 == 0 && count <= 1200) ||
                      // FLATCARDS (LIVE 20:09 census: the card batches are 4-41 cards of a SQUARE 256x256 BC3 picture, vs 776C74B8 -
                      // the thin-picture rule never matched): every textured additive card batch; the GS rebuilds only triangles lying
                      // flat to the eye's view (head-facing cards), anything else passes untouched
                      (textured && topo == D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST && count % 6 == 0 && count <= 3000);
    ID3D11GeometryShader* cur = nullptr;
    ctx->GSGetShader(&cur, nullptr, nullptr);
    const bool gameGs = cur != nullptr;
    if (cur) cur->Release();
    ID3D11VertexShader* vs = nullptr;
    ctx->VSGetShader(&vs, nullptr, nullptr);
    ID3D11GeometryShader* gs = quad && !gameGs ? gs_for(vs) : nullptr;
    if (g_passCount > 4000 && g_pieceKeyN < 64) { // census, once per distinct kind
        ID3D11ShaderResourceView* in[2] = {};
        ctx->PSGetShaderResources(0, 2, in);
        D3D11_TEXTURE2D_DESC t[2] = {};
        bool has[2] = {};
        for (int i = 0; i < 2; ++i) {
            ID3D11Resource* r = nullptr;
            if (in[i]) in[i]->GetResource(&r);
            has[i] = r && tex_desc(r, &t[i]);
            if (r) r->Release();
            if (in[i]) in[i]->Release();
        }
        uint64_t k = 1469598103934665603ull;
        const uint64_t parts[] = {uint64_t(indexed), uint64_t(count), uint64_t(topo), uint64_t(reinterpret_cast<uintptr_t>(vs)),
                                  has[0] ? (uint64_t(t[0].Width) << 32 | uint64_t(t[0].Height) << 8 | t[0].Format) : 0,
                                  has[1] ? (uint64_t(t[1].Width) << 32 | uint64_t(t[1].Height) << 8 | t[1].Format) : 0,
                                  uint64_t(b0.SrcBlend)};
        for (uint64_t p : parts) k = (k ^ p) * 1099511628211ull;
        bool known = false;
        for (int i = 0; i < g_pieceKeyN; ++i) known |= g_pieceKeys[i] == k;
        if (!known) {
            g_pieceKeys[g_pieceKeyN++] = k;
            BVR_LOG("[flareroll] glow draw %d: %s %u, topology %d, t0 %ux%u fmt %d, t1 %ux%u fmt %d, blend %d/%d, vs %p%s, %s", g_pieceKeyN,
                    indexed ? "indexed" : "draw", count, int(topo), has[0] ? t[0].Width : 0, has[0] ? t[0].Height : 0, has[0] ? int(t[0].Format) : 0,
                    has[1] ? t[1].Width : 0, has[1] ? t[1].Height : 0, has[1] ? int(t[1].Format) : 0, int(b0.SrcBlend), int(b0.DestBlend),
                    static_cast<void*>(vs), gameGs ? " (game GS)" : "", quad ? (gs ? "ALIGNED" : "quad, no shader") : "not aligned");
        }
    }
    if (vs) vs->Release();
    if (!quad || !gs) { if (quad && !gameGs) g_spriteNoGs.fetch_add(1, std::memory_order_relaxed); return 0; }
    if (g_hideTest.load(std::memory_order_relaxed)) { g_hidden.fetch_add(1, std::memory_order_relaxed); return 2; }
    ID3D11Device* dev = nullptr;
    ctx->GetDevice(&dev);
    if (!g_gsCb && dev) { D3D11_BUFFER_DESC cd{80, D3D11_USAGE_DEFAULT, D3D11_BIND_CONSTANT_BUFFER, 0, 0, 0}; dev->CreateBuffer(&cd, nullptr, &g_gsCb); }
    if (dev) dev->Release();
    if (!g_gsCb) return 0;
    D3D11_VIEWPORT vp{}; UINT nvp = 1;
    ctx->RSGetViewports(&nvp, &vp);
    if (!nvp || vp.Width < 2 || vp.Height < 2) return 0;
    const bool face = g_passFaceOk && g_faceGame.load(std::memory_order_relaxed); // FACEGAME: every card faces the game camera
    const float r[20] = {cosf(g_eyeAngle), sinf(g_eyeAngle), vp.Width * 0.5f, vp.Height * 0.5f,
                         g_passFace[0], g_passFace[1], g_passFace[2], g_passFace[3],
                         face ? 1.0f : 0.0f, 0, 0, 0,
                         g_passFace[4], g_passFace[5], g_passFace[6], 0,
                         g_passFace[7], g_passFace[8], g_passFace[9], 0};
    if (face) g_faced.fetch_add(1, std::memory_order_relaxed);
    bvr::frame_inspector::ScopedSuppress quiet;
    ctx->UpdateSubresource(g_gsCb, 0, nullptr, r, 0, 0);
    ctx->GSGetConstantBuffers(0, 1, &g_prevGsCb);
    ctx->GSSetShader(gs, nullptr, 0);
    ctx->GSSetConstantBuffers(0, 1, &g_gsCb);
    g_gsBound = true;
    (indexed ? g_indexedAligned : g_spriteDraws).fetch_add(1, std::memory_order_relaxed);
    return 1;
}
wchar_t g_path[MAX_PATH] = {};
} // namespace

void install() {
    // The device's CreateVertexShader (vtable slot 12, shared by every device of this runtime) from a throwaway device, before
    // the game creates its own: every vertex shader's outputs are noted for FLARESPRITE.
    ID3D11Device* dev = nullptr;
    const D3D_FEATURE_LEVEL fl = D3D_FEATURE_LEVEL_11_0;
    if (FAILED(D3D11CreateDevice(nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr, 0, &fl, 1, D3D11_SDK_VERSION, &dev, nullptr, nullptr)) || !dev) {
        BVR_LOG("[flareroll] no throwaway device - flare pieces stay as the game draws them");
        return;
    }
    void* target = (*reinterpret_cast<void***>(dev))[12];
    dev->Release();
    if (MH_CreateHook(target, reinterpret_cast<void*>(&create_vs_detour), reinterpret_cast<void**>(&g_origCreateVS)) != MH_OK ||
        MH_QueueEnableHook(target) != MH_OK || MH_ApplyQueued() != MH_OK) {
        BVR_LOG("[flareroll] CreateVertexShader hook FAILED - flare pieces stay as the game draws them");
        return;
    }
    BVR_LOG("[flareroll] CreateVertexShader hooked (%p): flare pieces can be turned level", target);
}

void set_eye_face(int eyeSign, const float lens[4], const float gameRight[3], const float gameUp[3]) {
    float f[10] = {lens[0], lens[1], lens[2], lens[3], gameRight[0], gameRight[1], gameRight[2], gameUp[0], gameUp[1], gameUp[2]};
    bool ok = std::fabs(f[0]) > 1e-4f && std::fabs(f[1]) > 1e-4f;
    for (float v : f) ok = ok && std::isfinite(v);
    AcquireSRWLockExclusive(&g_faceLock);
    memcpy(g_eyeFace[eyeSign < 0 ? 0 : 1], f, sizeof f);
    g_eyeFaceOk[eyeSign < 0 ? 0 : 1] = ok;
    ReleaseSRWLockExclusive(&g_faceLock);
}

void set_eye_angles(int eyeSign, float worldAngle, float headAngle) {
    float a = g_levelWorld.load(std::memory_order_relaxed) ? worldAngle : headAngle;
    if (!std::isfinite(a)) a = 0;
    g_angle[eyeSign < 0 ? 0 : 1].store(a, std::memory_order_relaxed);
}

void note_eye_pass(int eyeSign) {
    g_levelN = 0;
    if (g_heldDsv) { g_heldDsv->Release(); g_heldDsv = nullptr; } // never carried into another picture
    g_eyeAngle = eyeSign ? g_angle[eyeSign < 0 ? 0 : 1].load(std::memory_order_relaxed) : 0.0f;
    g_active = eyeSign != 0 && g_on.load(std::memory_order_relaxed) && fabsf(g_eyeAngle) >= kMinAngle;
    g_spritePass = eyeSign != 0 && g_on.load(std::memory_order_relaxed) && g_sprites.load(std::memory_order_relaxed);
    if (eyeSign) {
        AcquireSRWLockShared(&g_faceLock);
        memcpy(g_passFace, g_eyeFace[eyeSign < 0 ? 0 : 1], sizeof g_passFace);
        g_passFaceOk = g_eyeFaceOk[eyeSign < 0 ? 0 : 1];
        ReleaseSRWLockShared(&g_faceLock);
    }
    if (eyeSign) g_lastDeg.store(g_eyeAngle * 57.29578f, std::memory_order_relaxed);
    // CENSUS: the first eye picture of a launch tilted more than 3 deg lists every full-screen step once (what the next fix needs).
    if (g_census == 1) { g_census = 2; BVR_LOG("[flareroll] census end (%d steps listed)", g_censusLines); }
    ++g_passCount;
    if (g_census == 0 && g_active && fabsf(g_eyeAngle) > 0.052f && g_eyeW && g_passCount > 4000) { // ~40 s in: gameplay
        g_census = 1;
        BVR_LOG("[flareroll] census: eye %s, tilt %.1f deg, eye width %u - every full-screen step of this picture:", eyeSign < 0 ? "L" : "R",
                g_eyeAngle * 57.29578f, g_eyeW);
    }
}

void after_draw(ID3D11DeviceContext* ctx) {
    if (g_gsBound) { // FLARESPRITE: the game's (empty) geometry stage back
        bvr::frame_inspector::ScopedSuppress quiet;
        ctx->GSSetShader(nullptr, nullptr, 0);
        ctx->GSSetConstantBuffers(0, 1, &g_prevGsCb);
        if (g_prevGsCb) { g_prevGsCb->Release(); g_prevGsCb = nullptr; }
        g_gsBound = false;
    }
    if (!g_heldDsv) return;
    bvr::frame_inspector::ScopedSuppress quiet;
    ID3D11RenderTargetView* rts[8] = {};
    ctx->OMGetRenderTargets(8, rts, nullptr);
    ctx->OMSetRenderTargets(8, rts, g_heldDsv); // DEPTHOFF: the game's depth buffer back for its next draw
    for (auto* r : rts) if (r) r->Release();
    g_heldDsv->Release();
    g_heldDsv = nullptr;
}

bool before_draw_indexed(ID3D11DeviceContext* ctx, unsigned indexCount) {
    if (g_gsBound) after_draw(ctx);
    if (!g_spritePass) return false;
    return piece(ctx, indexCount, true) == 2;
}

void on_copy(ID3D11Resource* dst, ID3D11Resource* src) {
    if (!g_active || !dst) return;
    set_level(dst, src && is_level(src)); // a copy of level content is level; anything else written there is not
}

bool before_draw(ID3D11DeviceContext* ctx, unsigned vertexCount) {
    if (g_gsBound) after_draw(ctx); // a missed after-draw never leaves our geometry shader on the next draw
    if (g_spritePass) { // FLARESPRITE / PIECECENSUS
        const int r = piece(ctx, vertexCount, false);
        if (r == 2) return true;
        if (r == 1) return false;
    }
    if (!g_active || g_pipeFailed || (vertexCount != 3 && vertexCount != 4)) return false;
    ID3D11RenderTargetView* rtv = nullptr;
    ctx->OMGetRenderTargets(1, &rtv, nullptr);
    if (!rtv) return false;
    ID3D11Resource* rt = nullptr;
    rtv->GetResource(&rt);
    rtv->Release();
    D3D11_TEXTURE2D_DESC rd{};
    const bool rtOk = tex_desc(rt, &rd);
    if (rt) rt->Release(); // identity only from here
    if (!rtOk) return false;
    if (rd.Width > g_eyeW && rd.Width <= 8192 && rgba16(rd.Format)) g_eyeW = rd.Width;
    const bool quarterRt = g_eyeW && rd.Width * 3 <= g_eyeW && rd.Width >= 64;
    ID3D11ShaderResourceView* in[4] = {};
    ctx->PSGetShaderResources(0, 4, in);
    bool any = false;
    for (auto* v : in) any |= v != nullptr;
    if (!any) { if (quarterRt) set_level(rt, false); return false; }
    bool chain = g_chain.load(std::memory_order_relaxed) && quarterRt && rgba16(rd.Format);
    // DEPTHOFF (LIVE 17:54 / 17:55: "steps 0, depth-tested steps left 2985 / 6545" - every streak step runs against the quarter
    // depth-stencil T457, framedump_044343 "T524: 4 / 4"): a step with a REAL depth / stencil test would test the level picture
    // against the eye's own pixels, so its depth buffer is detached for that one draw (put back right after it).
    bool realTest = false;
    D3D11_DEPTH_STENCIL_DESC dd{};
    ID3D11DepthStencilView* dsv = nullptr;
    {
        ID3D11DepthStencilState* dss = nullptr; UINT ref = 0;
        ctx->OMGetDepthStencilState(&dss, &ref);
        ID3D11RenderTargetView* dummy = nullptr;
        ctx->OMGetRenderTargets(1, &dummy, &dsv);
        if (dummy) dummy->Release();
        if (dss) {
            dss->GetDesc(&dd);
            const auto face = [](const D3D11_DEPTH_STENCILOP_DESC& f) {
                return f.StencilFunc != D3D11_COMPARISON_ALWAYS || f.StencilPassOp != D3D11_STENCIL_OP_KEEP ||
                       f.StencilFailOp != D3D11_STENCIL_OP_KEEP || f.StencilDepthFailOp != D3D11_STENCIL_OP_KEEP;
            };
            realTest = dsv && ((dd.DepthEnable && (dd.DepthFunc != D3D11_COMPARISON_ALWAYS || dd.DepthWriteMask)) ||
                               (dd.StencilEnable && (face(dd.FrontFace) || face(dd.BackFace))));
            dss->Release();
        }
    }
    if (g_census == 1 && g_censusLines < 80) {
        ++g_censusLines;
        char srvs[4][48] = {};
        for (int i = 0; i < 4; ++i) {
            D3D11_TEXTURE2D_DESC d{}; ID3D11Resource* r = nullptr;
            if (in[i]) in[i]->GetResource(&r);
            if (r && tex_desc(r, &d)) snprintf(srvs[i], sizeof srvs[i], "%ux%u %s%s", d.Width, d.Height, fmt_name(d.Format), is_level(r) ? " L" : "");
            else strcpy_s(srvs[i], in[i] ? "?" : "-");
            if (r) r->Release();
        }
        BVR_LOG("[flareroll]  draw %u -> %ux%u %s%s | depth %d func %d write %d stencil %d (front %d/%d/%d/%d) real %d | t0 %s | t1 %s | t2 %s | t3 %s",
                vertexCount, rd.Width, rd.Height, fmt_name(rd.Format), chain ? " CHAIN" : quarterRt ? " quarter" : "", int(dd.DepthEnable),
                int(dd.DepthFunc), int(dd.DepthWriteMask), int(dd.StencilEnable), int(dd.FrontFace.StencilFunc), int(dd.FrontFace.StencilPassOp),
                int(dd.FrontFace.StencilFailOp), int(dd.FrontFace.StencilDepthFailOp), realTest ? 1 : 0, srvs[0], srvs[1], srvs[2], srvs[3]);
    }
    if (chain && realTest) {
        g_skippedDepth.fetch_add(1, std::memory_order_relaxed);
        ID3D11RenderTargetView* rts[8] = {};
        ctx->OMGetRenderTargets(8, rts, nullptr);
        ctx->OMSetRenderTargets(8, rts, nullptr);
        for (auto* r : rts) if (r) r->Release();
        if (g_heldDsv) g_heldDsv->Release();
        g_heldDsv = dsv; dsv = nullptr; // after_draw binds it back
    }
    if (dsv) dsv->Release();
    // Inputs to turn: in a quarter-size step, every RGBA16 input still in the eye's picture -> level; anywhere else, every level
    // input -> back to the eye's roll.
    ID3D11ShaderResourceView* repl[4] = {};
    bool need = false;
    D3D11_TEXTURE2D_DESC sd[4] = {};
    for (int i = 0; i < 4; ++i) {
        if (!in[i]) continue;
        ID3D11Resource* r = nullptr; in[i]->GetResource(&r);
        const bool ok = tex_desc(r, &sd[i]) && sd[i].MipLevels == 1 && sd[i].ArraySize == 1;
        const bool lvl = is_level(r);
        if (r) r->Release();
        if (!ok) continue;
        if (chain ? (!lvl && rgba16(sd[i].Format)) : lvl) { repl[i] = in[i]; need = true; }
    }
    if (need) {
        bvr::frame_inspector::ScopedSuppress quiet;
        Saved s;
        s.save(ctx);
        ID3D11Device* dev = nullptr; ctx->GetDevice(&dev);
        const bool ready = dev && ensure_pipeline(dev);
        if (dev) dev->Release();
        if (ready) {
            s.ours(ctx);
            for (int i = 0; i < 4; ++i)
                if (repl[i]) {
                    repl[i] = turned(ctx, repl[i], sd[i], chain ? g_eyeAngle : -g_eyeAngle);
                    (chain ? g_rotIn : g_rotOut).fetch_add(1, std::memory_order_relaxed);
                }
        }
        s.restore(ctx);
        ID3D11ShaderResourceView* use[4];
        for (int i = 0; i < 4; ++i) use[i] = repl[i] ? repl[i] : in[i];
        ctx->PSSetShaderResources(0, 4, use); // the game's draw reads our copies (and its own inputs, which our passes unbound)
    }
    for (auto* v : in) if (v) v->Release();
    if (chain) { set_level(rt, true); g_passes.fetch_add(1, std::memory_order_relaxed); }
    else if (quarterRt) set_level(rt, false);
    return false;
}

void draw_ui() {
    bool on = g_on.load();
    if (ImGui::Checkbox("keep lens-flare streaks level when you tilt your head", &on)) { g_on.store(on); if (g_path[0]) settings_save(g_path); }
    if (on) {
        bool lw = g_levelWorld.load();
        if (ImGui::Checkbox("   level with the game world  (off = level with the real room)", &lw)) { g_levelWorld.store(lw); if (g_path[0]) settings_save(g_path); }
    }
    if (bvr::overlay::show_tests()) {
        ImGui::TextDisabled("   flare pieces turned %u (no shader %u), shaders built %u / failed %u, noted %u, last tilt %.1f deg",
                            g_spriteDraws.load(), g_spriteNoGs.load(), g_gsBuilt.load(), g_gsFailed.load(), g_vsNoted.load(), g_lastDeg.load());
        bool ch = g_chain.load();
        if (ImGui::Checkbox("   also turn the quarter-size glow steps (test)", &ch)) g_chain.store(ch);
        bool fg = g_faceGame.load();
        if (ImGui::Checkbox("   streak cards face the game camera, not your head (FACEGAME)", &fg)) g_faceGame.store(fg);
        ImGui::TextDisabled("   cards turned to the game camera %u", g_faced.load());
        bool hd = g_hideTest.load();
        if (ImGui::Checkbox("   hide the flare pieces the mod turns (test: does the streak vanish?)", &hd)) g_hideTest.store(hd);
        ImGui::TextDisabled("   hidden %u, indexed pieces aligned %u", g_hidden.load(), g_indexedAligned.load());
    }
}
void status_line(char* out, unsigned n) {
    snprintf(out, n, "flareroll %s: flare pieces turned %u + batches %u (no shader %u; facing the game camera %u), shaders built %u / failed %u, "
             "vertex shaders noted %u | chain %d: steps %u, turned in %u / back %u, depth detached %u, failed %u | last tilt %.1f deg",
             g_on.load() ? "ON" : "off", g_spriteDraws.load(), g_indexedAligned.load(), g_spriteNoGs.load(), g_faced.load(), g_gsBuilt.load(),
             g_gsFailed.load(), g_vsNoted.load(), g_chain.load() ? 1 : 0,
             g_passes.load(), g_rotIn.load(), g_rotOut.load(), g_skippedDepth.load(), g_failed.load(), g_lastDeg.load());
}
bool command(const char* args) {
    if (!strcmp(args, "on")) g_on.store(true);
    else if (!strcmp(args, "off")) g_on.store(false);
    else return false;
    char s[200]; status_line(s, sizeof s); BVR_LOG("[flareroll] %s", s);
    if (g_path[0]) settings_save(g_path);
    return true;
}
void settings_load(const wchar_t* path) {
    wcsncpy_s(g_path, path, _TRUNCATE);
    g_on.store(GetPrivateProfileIntW(L"lens_flares", L"level", 1, path) != 0);
    g_levelWorld.store(GetPrivateProfileIntW(L"lens_flares", L"level_world2", 0, path) != 0); // new key: the old default was the world
    BVR_LOG("[flareroll] loaded: lens-flare streaks kept level %s", g_on.load() ? "ON" : "off");
}
void settings_save(const wchar_t* path) {
    WritePrivateProfileStringW(L"lens_flares", L"level", g_on.load() ? L"1" : L"0", path);
    WritePrivateProfileStringW(L"lens_flares", L"level_world2", g_levelWorld.load() ? L"1" : L"0", path);
}
} // namespace bvr::bo::flare_roll
