#pragma once
// EYEBURST: record what goes to the headset for N presents (see eye_burst.cpp).
#include <cstdint>

struct ID3D11DeviceContext;
struct ID3D11Texture2D;

namespace bvr::vr::eye_burst {
void start(int presents); // any thread: the next present begins recording
bool active();             // a recording is requested or running
void on_present(ID3D11DeviceContext* ctx, ID3D11Texture2D* backbuffer, int srSign, uint32_t gen, bool afw); // render thread
void on_final(ID3D11DeviceContext* ctx, ID3D11Texture2D* image, int eye); // a finished headset image, before its release
}
