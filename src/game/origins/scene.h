#pragma once
struct IDXGISwapChain;
namespace bvr::bo::scene {
bool install();
bool hook_live();
bool handle_command(const char* args);
void draw_ui();
void on_present(IDXGISwapChain* sc);
void set_camera(bool on);
void set_stereo(bool on);
}
