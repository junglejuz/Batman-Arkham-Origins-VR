#pragma once
// AIMDEPTH (JJ 2026-10-27 night: the batclaw "is still offset from the reticle to up and right"): the point you look at, from the left
// eye's own depth where the head points (Batman City markers.cpp AIMDEPTH; VR_ESSENTIALS §6 "Centre aim reticles: find their point
// by ray-marching the depth buffer"). Head aim then steers the controller so the GAME camera's ray - from over his right shoulder -
// passes through that point instead of running parallel to ours.
struct ID3D11DeviceContext;

namespace bvr::bo::aim_depth {
// Game thread, the scene view constructor (final eye view): where the eye's centre ray lands in its picture (pixels), the lens'
// depth terms (z = m32 / (depth - m22)), and the eye's world origin + forward.
void set_eye(int eyeSign, float px, float py, float m22, float m32, const float origin[3], const float forward[3]);
// Render thread: a new eye picture starts.
void note_eye_pass(int eyeSign);
// Render thread, before every non-indexed draw: at the eye's tone-map step, the depth under the centre ray is read (never drops it).
void before_draw(ID3D11DeviceContext* ctx, unsigned vertexCount);
// Game thread: the latest looked-at world point (left eye) if read within the last 150 ms and farther than 1 m.
bool point(float out[3]);
void status_line(char* out, unsigned n);
} // namespace bvr::bo::aim_depth
