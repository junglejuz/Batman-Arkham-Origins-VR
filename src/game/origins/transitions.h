#pragma once
#include <cstdint>
struct ID3D11DeviceContext;
struct ID3D11Texture2D;
struct IDXGISwapChain;
namespace bvr::bo::transitions {
bool install();
void draw_ui();
bool command(const char*);
void status();
void save_capture(ID3D11DeviceContext*, const wchar_t* directory);
}
