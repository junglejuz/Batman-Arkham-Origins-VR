#include <windows.h>
#include <cstdio>
#include <Xinput.h>
#include <d3d11.h>
#include <dxgi.h>
int wmain(int argc,wchar_t** argv) {
    if(argc==4 && wcscmp(argv[3],L"flat")==0) {
        if(!LoadLibraryW(argv[2])) return 1;
        Sleep(100);
        if(GetModuleHandleW(L"batman_origins.dll")) return 2;
        printf("PASS: ordinary launch forwards XInput without loading the VR mod.\n");
        return 0;
    }
    if(argc!=3) return 1;
    HMODULE mod=LoadLibraryW(argv[1]);
    if(!mod) { printf("mod load failed %lu\n",GetLastError()); return 2; }
    using Identity=DWORD(WINAPI*)();
    auto identity=reinterpret_cast<Identity>(GetProcAddress(mod,"_BO_Identity@0"));
    if(!identity) identity=reinterpret_cast<Identity>(GetProcAddress(mod,"BO_Identity"));
    if(!identity) return 3;
    for(int i=0;i<100 && identity()==0;++i) Sleep(10);
    if(identity()!=2) return 4;
    auto forwarding=reinterpret_cast<Identity>(GetProcAddress(mod,"_BO_ForwardingSelftest@0"));
    if(!forwarding) forwarding=reinterpret_cast<Identity>(GetProcAddress(mod,"BO_ForwardingSelftest"));
    if(!forwarding || forwarding()!=0) return 11;
    using HudCheck=DWORD(WINAPI*)(ID3D11DeviceContext*,IDXGISwapChain*);
    auto check=reinterpret_cast<HudCheck>(GetProcAddress(mod,"_BO_HudBoundarySelftest@8"));
    if(!check) return 12;
    HWND wnd=CreateWindowExW(0,L"STATIC",L"Origins offline HUD check",WS_OVERLAPPEDWINDOW,0,0,64,64,nullptr,nullptr,GetModuleHandleW(nullptr),nullptr);
    DXGI_SWAP_CHAIN_DESC sd{}; sd.BufferDesc.Width=64; sd.BufferDesc.Height=64;
    sd.BufferDesc.Format=DXGI_FORMAT_R8G8B8A8_UNORM; sd.SampleDesc.Count=1;
    sd.BufferUsage=DXGI_USAGE_RENDER_TARGET_OUTPUT; sd.BufferCount=1; sd.OutputWindow=wnd; sd.Windowed=TRUE;
    ID3D11Device* dev=nullptr; ID3D11DeviceContext* ctx=nullptr; IDXGISwapChain* sc=nullptr;
    if(FAILED(D3D11CreateDeviceAndSwapChain(nullptr,D3D_DRIVER_TYPE_WARP,nullptr,0,nullptr,0,D3D11_SDK_VERSION,&sd,&sc,&dev,nullptr,&ctx))) return 13;
    const DWORD hudResult=check(ctx,sc);
    auto batCheck=reinterpret_cast<HudCheck>(GetProcAddress(mod,"_BO_TransitionSelftest@8"));
    const DWORD batResult=batCheck ? batCheck(ctx,sc) : 1;
    ctx->Release(); sc->Release(); dev->Release(); DestroyWindow(wnd);
    if(hudResult) { printf("HUD classifier check failed %lu\n",hudResult); return 14; }
    if(batResult) { printf("Transition render/forwarding check failed %lu\n",batResult); return 15; }
    HMODULE proxy=LoadLibraryW(argv[2]);
    if(!proxy) return 5;
    const char* names[]={"XInputGetState","XInputSetState","XInputGetCapabilities","XInputEnable",
       "XInputGetDSoundAudioDeviceGuids","XInputGetBatteryInformation","XInputGetKeystroke","BVR_SetPostGetStateHook"};
    for(auto* name:names) if(!GetProcAddress(proxy,name)) { printf("missing %s\n",name); return 6; }
    using Get=DWORD(WINAPI*)(DWORD,XINPUT_STATE*);
    auto get=reinterpret_cast<Get>(GetProcAddress(proxy,"XInputGetState"));
    auto getex=reinterpret_cast<Get>(GetProcAddress(proxy,MAKEINTRESOURCEA(100)));
    if(!getex) return 7;
    wchar_t path[MAX_PATH]{}; GetSystemDirectoryW(path,MAX_PATH); wcscat_s(path,L"\\xinput1_3.dll");
    HMODULE real=LoadLibraryW(path); if(!real) return 8;
    auto systemGet=reinterpret_cast<Get>(GetProcAddress(real,"XInputGetState"));
    XINPUT_STATE a{},b{};
    if(get(99,&a)!=systemGet(99,&b)) return 9;
    auto systemEx=reinterpret_cast<Get>(GetProcAddress(real,MAKEINTRESOURCEA(100)));
    if(systemEx && getex(99,&a)!=systemEx(99,&b)) return 10;
    printf("PASS: foreign host refused; constructor/lens; HUD boundary; actual bat recording, WARP black/animated cover/reveal, typed and typeless RGBA/BGRA runtime transfer, loading handoff, state restoration and three audio wrappers; proxy forwarding.\n");
    return 0;
}
