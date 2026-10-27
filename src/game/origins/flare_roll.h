#pragma once
// FLAREROLL (JJ 2026-10-27: "remove [the glow] and put back the original lens flare and fix it so that lens flares do not rotate
// with head rolls"): the game's lens-flare streaks are blurred along the SCREEN's x axis at quarter size, so they tilt with the head.
// Around the game's own quarter-size steps (streak + bloom, R16G16B16A16 targets), every input that is still in the eye's picture is
// first turned LEVEL with the game camera's horizon; the game then draws its horizontal streak; where the final picture reads those
// results they are turned back to the eye's roll. Nothing else changes; with the head level it does nothing at all.
struct ID3D11DeviceContext;
struct ID3D11Resource;

namespace bvr::bo::flare_roll {
// At start-up, before the game creates its device: hooks CreateVertexShader (FLARESPRITE).
void install();
// Game thread, the scene view constructor: this eye's tilt as a picture angle (radians, pixels, y down) - against the game world's
// horizon (relative to the game camera's own) and against real-world level (the headset); the panel picks which one is used.
void set_eye_angles(int eyeSign, float worldAngle, float headAngle);
// Game thread, the scene view constructor (FACEGAME): this eye's lens terms (P00, P11, P20, P21) and the game camera's right / up
// expressed in this eye's view space.
void set_eye_face(int eyeSign, const float lens[4], const float gameRight[3], const float gameUp[3]);
// Render thread, the engine's view-family begin: a new eye picture starts.
void note_eye_pass(int eyeSign);
// Render thread, before every non-indexed draw. True = drop it (only the TESTS "hide the flare pieces" switch).
bool before_draw(ID3D11DeviceContext* ctx, unsigned vertexCount);
// Render thread, right after every non-indexed draw: a depth buffer detached for that draw goes back (DEPTHOFF).
void after_draw(ID3D11DeviceContext* ctx);
// Render thread, before / after every DrawIndexed (PIECECENSUS: single indexed quads aligned too). True = drop (TESTS hide switch).
bool before_draw_indexed(ID3D11DeviceContext* ctx, unsigned indexCount);
// Render thread, every game copy (before it runs).
void on_copy(ID3D11Resource* dst, ID3D11Resource* src);
void draw_ui();
void status_line(char* out, unsigned n);
bool command(const char* args);          // "flareroll on|off"
void settings_load(const wchar_t* path); // settings.ini [lens_flares] level
void settings_save(const wchar_t* path);
} // namespace bvr::bo::flare_roll
