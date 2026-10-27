#pragma once
// DETVR for Origins (JJ 2026-10-27 night: "in detective mode, there seems to be a vignette around each eye independently. There should
// just be one vignette around both fused eyes"). Batman City's fix (vision.cpp DETVR, JJ "This is perfect"): the detective screen
// material lays its overlay layers (scanlines, honeycomb + vignette, grain) on each eye's own off-centre screen; it is rebuilt with the
// overlays sampled at a view direction shared by both eyes. Origins' material is not identified yet: this module records every pixel
// shader created with City's detective input signature (bytecode saved to %LOCALAPPDATA%\...\detective\<checksum>.dxbc, log
// "[bo] DETVR candidate"), logs which of them detective mode draws, and offers City's rebuild on them as a TESTS switch.
struct ID3D11DeviceContext;

namespace bvr::bo::detective {
void install();                                                  // before the game creates its device
bool before_draw(ID3D11DeviceContext* ctx, unsigned vertexCount); // render thread, every Draw; never drops it (returns false)
void after_draw(ID3D11DeviceContext* ctx);                       // render thread, after every Draw
void set_render_pass(int eyeSign);                               // render thread, BeginDrawing (-1 left, +1 right, 0 mono)
bool scanner_active();                                           // SCANHUD: the evidence scanner was drawn in the last 300 ms
bool draw_ui();                                                  // HUD drawer; true = changed (save)
bool enabled();
void set_enabled(bool on);
} // namespace bvr::bo::detective
