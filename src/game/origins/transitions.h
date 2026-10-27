#pragma once
#include <cstdint>
struct ID3D11DeviceContext;
struct ID3D11Texture2D;
struct IDXGISwapChain;
namespace bvr::bo::transitions {
bool install();
// ENTRYHOLD: every finished eye image black until GetTickCount64() + ms.
void hold_black(unsigned ms);
void draw_ui();      // player settings (MENUS AND SCREENS)
void draw_test_ui(); // test buttons (TESTS drawer)
bool command(const char*);
void status();
void save_capture(ID3D11DeviceContext*, const wchar_t* directory);
}
