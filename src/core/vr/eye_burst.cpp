// EYEBURST (2026-10-13, JJ: "the only annoying thing now with origins is the flickering duplicate in the map/tab menu").
// Seven map fixes failed on 2026-10-11 because every one guessed the route from counters; the notes ask for an objective
// record first ("an objective per-eye picture burst in the map ... to find the frames that hold the copy, then name their
// draws - before any further fix"). For N presents this saves, per present, a small copy of the game window picture
// (= what goes into that eye, before DLSS) and of the HUD sheet (= the menu layer), plus one index line (present number,
// eye sign, pose generation, AFW, per-present counters and states). EYEBURST2 (after JJ 17:45 "the map became attached to
// my face ... the map just stays there when I exit"): also every FINISHED eye image as it goes to the headset (after
// DLSS, the held-left finish, AFW, the bat paint) - final.raw + final.txt - so a stuck picture is found wherever it lives.
// Copies go through small rings of staging textures read a few copies later, so the game never waits on the GPU for them.
// Files: captures\eyeburst_<time>\ (RGB, 1/8 size, one frame after another). tools\eyeburst_view.py reads them.
#include "core/vr/eye_burst.h"

#include "core/gfx/gfx_hud.h"
#include "core/util/log.h"
#include "core/vr/openxr_runtime.h"

#include <windows.h>
#include <d3d11.h>

#include <atomic>
#include <cstdio>
#include <vector>

namespace bvr::vr::eye_burst {
namespace {

constexpr int kRing = 4;
constexpr int kStep = 8; // keep every 8th pixel in x and y (three streams per present)

struct Slot {
    ID3D11Texture2D* eye = nullptr;
    ID3D11Texture2D* hud = nullptr;
    bool used = false;
    char line[320] = {};
};
struct FinalSlot {
    ID3D11Texture2D* tex = nullptr;
    bool used = false;
    char line[96] = {};
};

std::atomic<int> g_request{0};
int g_left = 0;          // presents still to record (render thread)
uint64_t g_seq = 0;
Slot g_ring[kRing];
FinalSlot g_fring[kRing];
uint64_t g_fseq = 0;
D3D11_TEXTURE2D_DESC g_eyeDesc{}, g_hudDesc{}, g_finDesc{};
FILE* g_eyeF = nullptr;
FILE* g_hudF = nullptr;
FILE* g_idxF = nullptr;
FILE* g_finF = nullptr;
FILE* g_finIdxF = nullptr;
std::vector<unsigned char> g_row;

void release_ring() {
    for (Slot& s : g_ring) {
        if (s.eye) s.eye->Release();
        if (s.hud) s.hud->Release();
        s = Slot{};
    }
    for (FinalSlot& s : g_fring) {
        if (s.tex) s.tex->Release();
        s = FinalSlot{};
    }
    g_finDesc = D3D11_TEXTURE2D_DESC{};
}

bool same_shape(const D3D11_TEXTURE2D_DESC& a, const D3D11_TEXTURE2D_DESC& b) {
    return a.Width == b.Width && a.Height == b.Height && a.Format == b.Format;
}

ID3D11Texture2D* make_staging(ID3D11Device* dev, const D3D11_TEXTURE2D_DESC& src) {
    D3D11_TEXTURE2D_DESC d{};
    d.Width = src.Width; d.Height = src.Height; d.MipLevels = 1; d.ArraySize = 1;
    d.Format = src.Format; d.SampleDesc = {1, 0};
    d.Usage = D3D11_USAGE_STAGING; d.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
    ID3D11Texture2D* t = nullptr;
    dev->CreateTexture2D(&d, nullptr, &t);
    return t;
}

// One pixel -> 8-bit RGB for the formats the game window / HUD sheet / headset images use.
bool to_rgb(DXGI_FORMAT f, const unsigned char* p, unsigned char out[3]) {
    switch (f) {
    case DXGI_FORMAT_R8G8B8A8_UNORM: case DXGI_FORMAT_R8G8B8A8_UNORM_SRGB: case DXGI_FORMAT_R8G8B8A8_TYPELESS:
        out[0] = p[0]; out[1] = p[1]; out[2] = p[2]; return true;
    case DXGI_FORMAT_B8G8R8A8_UNORM: case DXGI_FORMAT_B8G8R8A8_UNORM_SRGB: case DXGI_FORMAT_B8G8R8A8_TYPELESS:
        out[0] = p[2]; out[1] = p[1]; out[2] = p[0]; return true;
    case DXGI_FORMAT_R10G10B10A2_UNORM: case DXGI_FORMAT_R10G10B10A2_TYPELESS: {
        const uint32_t v = *reinterpret_cast<const uint32_t*>(p);
        out[0] = (unsigned char)((v & 1023) >> 2); out[1] = (unsigned char)(((v >> 10) & 1023) >> 2);
        out[2] = (unsigned char)(((v >> 20) & 1023) >> 2);
        return true;
    }
    default: return false;
    }
}
unsigned bytes_per_pixel(DXGI_FORMAT f) {
    return (f == DXGI_FORMAT_R16G16B16A16_FLOAT) ? 8 : 4;
}

void write_frame(ID3D11DeviceContext* ctx, ID3D11Texture2D* tex, const D3D11_TEXTURE2D_DESC& desc, FILE* f) {
    const unsigned w = desc.Width / kStep, h = desc.Height / kStep;
    g_row.assign(size_t(w) * 3, 0);
    D3D11_MAPPED_SUBRESOURCE m{};
    const bool ok = tex && SUCCEEDED(ctx->Map(tex, 0, D3D11_MAP_READ, 0, &m));
    const unsigned bpp = bytes_per_pixel(desc.Format);
    for (unsigned y = 0; y < h; ++y) {
        if (ok) {
            const unsigned char* row = static_cast<const unsigned char*>(m.pData) + size_t(y) * kStep * m.RowPitch;
            for (unsigned x = 0; x < w; ++x)
                if (!to_rgb(desc.Format, row + size_t(x) * kStep * bpp, &g_row[size_t(x) * 3]))
                    g_row[size_t(x) * 3] = g_row[size_t(x) * 3 + 1] = g_row[size_t(x) * 3 + 2] = 255; // unknown format: white
        }
        fwrite(g_row.data(), 1, g_row.size(), f);
    }
    if (ok) ctx->Unmap(tex, 0);
}

void flush_slot(ID3D11DeviceContext* ctx, Slot& s) {
    if (!s.used) return;
    write_frame(ctx, s.eye, g_eyeDesc, g_eyeF);
    write_frame(ctx, s.hud, g_hudDesc, g_hudF);
    fputs(s.line, g_idxF);
    s.used = false;
}
void flush_final(ID3D11DeviceContext* ctx, FinalSlot& s) {
    if (!s.used) return;
    write_frame(ctx, s.tex, g_finDesc, g_finF);
    fputs(s.line, g_finIdxF);
    s.used = false;
}

void finish(ID3D11DeviceContext* ctx) {
    for (int k = 0; k < kRing; ++k) flush_slot(ctx, g_ring[(g_seq + k) % kRing]);
    for (int k = 0; k < kRing; ++k) flush_final(ctx, g_fring[(g_fseq + k) % kRing]);
    if (g_finIdxF && g_finDesc.Width)
        fprintf(g_finIdxF, "# final %ux%u fmt %d, step %d\n", g_finDesc.Width, g_finDesc.Height, int(g_finDesc.Format), kStep);
    for (FILE** f : {&g_eyeF, &g_hudF, &g_idxF, &g_finF, &g_finIdxF})
        if (*f) { fclose(*f); *f = nullptr; }
    release_ring();
    BVR_LOG("[eyeburst] done: %llu presents, %llu finished headset images recorded", (unsigned long long)g_seq,
            (unsigned long long)g_fseq);
}

bool begin(ID3D11Device* dev, ID3D11Texture2D* backbuffer) {
    backbuffer->GetDesc(&g_eyeDesc);
    ID3D11Texture2D* hud = gfx_hud::hud_sheet_texture();
    if (hud) hud->GetDesc(&g_hudDesc); else g_hudDesc = g_eyeDesc;
    if (bytes_per_pixel(g_eyeDesc.Format) != 4) {
        BVR_LOG("[eyeburst] window picture format %d not readable - not started", int(g_eyeDesc.Format));
        return false;
    }
    SYSTEMTIME st; GetLocalTime(&st);
    wchar_t dir[MAX_PATH];
    swprintf_s(dir, L"%s\\captures", log::data_dir());
    CreateDirectoryW(dir, nullptr);
    swprintf_s(dir, L"%s\\captures\\eyeburst_%02d%02d%02d", log::data_dir(), st.wHour, st.wMinute, st.wSecond);
    CreateDirectoryW(dir, nullptr);
    wchar_t p[MAX_PATH];
    swprintf_s(p, L"%s\\eye.raw", dir); _wfopen_s(&g_eyeF, p, L"wb");
    swprintf_s(p, L"%s\\hud.raw", dir); _wfopen_s(&g_hudF, p, L"wb");
    swprintf_s(p, L"%s\\index.txt", dir); _wfopen_s(&g_idxF, p, L"w");
    swprintf_s(p, L"%s\\final.raw", dir); _wfopen_s(&g_finF, p, L"wb");
    swprintf_s(p, L"%s\\final.txt", dir); _wfopen_s(&g_finIdxF, p, L"w");
    if (!g_eyeF || !g_hudF || !g_idxF || !g_finF || !g_finIdxF) {
        BVR_LOG("[eyeburst] could not open the files - not started");
        for (FILE** f : {&g_eyeF, &g_hudF, &g_idxF, &g_finF, &g_finIdxF})
            if (*f) { fclose(*f); *f = nullptr; }
        return false;
    }
    fprintf(g_idxF, "# eye %ux%u fmt %d, hud %ux%u fmt %d, step %d (files hold 1/%d size RGB)\n", g_eyeDesc.Width,
            g_eyeDesc.Height, int(g_eyeDesc.Format), g_hudDesc.Width, g_hudDesc.Height, int(g_hudDesc.Format), kStep, kStep);
    fprintf(g_idxF, "# seq present_ms sign gen afw | early_caught early_undone second_blits mapsafe_starts mapsafe_nopass "
                    "mapsafe_skips redirect hud_menu_now hud_head_locked\n");
    fprintf(g_finIdxF, "# fseq eye present_seq (the eye present it was finished at)\n");
    for (Slot& s : g_ring) {
        s.eye = make_staging(dev, g_eyeDesc);
        s.hud = hud ? make_staging(dev, g_hudDesc) : nullptr;
    }
    g_seq = 0;
    g_fseq = 0;
    BVR_LOG("[eyeburst] recording %d presents: eye %ux%u fmt %d, HUD sheet %ux%u fmt %d, + finished headset images -> %ls",
            g_left, g_eyeDesc.Width, g_eyeDesc.Height, int(g_eyeDesc.Format), g_hudDesc.Width, g_hudDesc.Height,
            int(g_hudDesc.Format), dir);
    return true;
}

} // namespace

bool active() { return g_left > 0 || g_request.load(std::memory_order_relaxed) > 0; }

void start(int presents) { g_request.store(presents < 1 ? 1 : presents > 2400 ? 2400 : presents); }

void on_present(ID3D11DeviceContext* ctx, ID3D11Texture2D* backbuffer, int srSign, uint32_t gen, bool afw) {
    if (!ctx || !backbuffer) return;
    if (const int req = g_request.exchange(0)) {
        if (g_left > 0) finish(ctx);
        g_left = req;
        ID3D11Device* dev = nullptr;
        ctx->GetDevice(&dev);
        const bool ok = dev && begin(dev, backbuffer);
        if (dev) dev->Release();
        if (!ok) { g_left = 0; release_ring(); return; }
    }
    if (g_left <= 0) return;
    D3D11_TEXTURE2D_DESC d{};
    backbuffer->GetDesc(&d);
    if (!same_shape(d, g_eyeDesc)) { BVR_LOG("[eyeburst] window picture changed size - stopped"); g_left = 0; finish(ctx); return; }
    Slot& s = g_ring[g_seq % kRing];
    flush_slot(ctx, s); // the frame recorded kRing presents ago: its copy is long done
    ctx->CopyResource(s.eye, backbuffer);
    ID3D11Texture2D* hud = gfx_hud::hud_sheet_texture();
    D3D11_TEXTURE2D_DESC hd{};
    if (hud) hud->GetDesc(&hd);
    if (hud && s.hud && same_shape(hd, g_hudDesc)) ctx->CopyResource(s.hud, hud);
    uint32_t early = 0, undone = 0;
    gfx_hud::early_ui_stats(&early, &undone);
    uint32_t msStart = 0, msNoPass = 0, msSkip = 0;
    gfx_hud::map_safe_stats(&msStart, &msNoPass, &msSkip);
    LARGE_INTEGER q, f; QueryPerformanceCounter(&q); QueryPerformanceFrequency(&f);
    snprintf(s.line, sizeof(s.line), "%llu %.3f %d %u %d | %u %u %u %u %u %u %d %d %d\n", (unsigned long long)g_seq,
             double(q.QuadPart) * 1000.0 / double(f.QuadPart), srSign, gen, afw ? 1 : 0, early, undone,
             gfx_hud::second_blits(), msStart, msNoPass, msSkip, gfx_hud::redirect_on() ? 1 : 0, hud_menu_now() ? 1 : 0,
             hud_head_locked() ? 1 : 0);
    s.used = true;
    ++g_seq;
    if (--g_left == 0) finish(ctx);
}

void on_final(ID3D11DeviceContext* ctx, ID3D11Texture2D* image, int eye) {
    if (g_left <= 0 || !ctx || !image || !g_finF) return;
    D3D11_TEXTURE2D_DESC d{};
    image->GetDesc(&d);
    if (!g_finDesc.Width) { // first finished image of this recording: size the ring after it
        g_finDesc = d;
        ID3D11Device* dev = nullptr;
        ctx->GetDevice(&dev);
        for (FinalSlot& s : g_fring) s.tex = dev ? make_staging(dev, d) : nullptr;
        if (dev) dev->Release();
    }
    if (!same_shape(d, g_finDesc)) return;
    FinalSlot& s = g_fring[g_fseq % kRing];
    flush_final(ctx, s);
    if (!s.tex) return;
    ctx->CopyResource(s.tex, image);
    snprintf(s.line, sizeof(s.line), "%llu %d %llu\n", (unsigned long long)g_fseq, eye,
             (unsigned long long)(g_seq ? g_seq - 1 : 0));
    s.used = true;
    ++g_fseq;
}

} // namespace bvr::vr::eye_burst
