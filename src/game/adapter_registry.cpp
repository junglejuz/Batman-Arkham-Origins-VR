#include "adapter_registry.h"
#include "igame_adapter.h"
#include "core/util/log.h"
#include <windows.h>
namespace bvr::bo { bvr::game::IGameAdapter* create_adapter(); }
namespace bvr::game {
namespace { IGameAdapter* current = nullptr; }
HostGame detect_host_game() {
    wchar_t path[MAX_PATH]{}; GetModuleFileNameW(nullptr,path,MAX_PATH);
    auto* p=wcsrchr(path,L'\\');
    return _wcsicmp(p ? p+1 : path,L"BatmanOrigins.exe") == 0 ? HostGame::ArkhamCity : HostGame::Unknown;
}
const wchar_t* host_data_subdir() { return L""; }
void init_adapter() {
    if(detect_host_game()==HostGame::Unknown) return;
    current=bvr::bo::create_adapter();
    pattern_scan::ProcessImage image{};
    if(pattern_scan::capture_main_module(image)) current->init(image);
}
IGameAdapter* adapter() { return current; }
}
