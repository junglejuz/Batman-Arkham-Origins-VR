// City monitor-cap method, applied to Origins imports; no donor instruction patches.
#include "window.h"
#include "core/util/log.h"
#include <windows.h>
#include <MinHook.h>
#include <cstdlib>
#include <cstring>
namespace bvr::bo::window {
namespace {
HWND game=nullptr;
WNDPROC previous=nullptr;
unsigned width=0,height=0;
using Create=HWND(WINAPI*)(DWORD,LPCWSTR,LPCWSTR,DWORD,int,int,int,int,HWND,HMENU,HINSTANCE,LPVOID);
using Position=BOOL(WINAPI*)(HWND,HWND,int,int,int,int,UINT);
using Metrics=int(WINAPI*)(int);
using Monitor=BOOL(WINAPI*)(HMONITOR,LPMONITORINFO);
using Work=BOOL(WINAPI*)(UINT,UINT,PVOID,UINT);
Create createOriginal=nullptr;
Position positionOriginal=nullptr;
Metrics metricsOriginal=nullptr;
Monitor monitorOriginal=nullptr;
Work workOriginal=nullptr;
void grow(RECT& r) { r.right=r.left+8192; r.bottom=r.top+8192; }
int WINAPI metrics(int i) {
    int v=metricsOriginal(i);
    switch(i) {
    case SM_CXSCREEN:case SM_CYSCREEN:case SM_CXFULLSCREEN:case SM_CYFULLSCREEN:
    case SM_CXMAXTRACK:case SM_CYMAXTRACK:case SM_CXVIRTUALSCREEN:case SM_CYVIRTUALSCREEN:
        return v<8192 ? 8192 : v;
    default:return v;
    }
}
BOOL WINAPI monitor(HMONITOR m,LPMONITORINFO info) {
    BOOL out=monitorOriginal(m,info); if(out && info) { grow(info->rcMonitor); grow(info->rcWork); } return out;
}
BOOL WINAPI work(UINT a,UINT b,PVOID p,UINT flags) {
    BOOL out=workOriginal(a,b,p,flags); if(out && a==SPI_GETWORKAREA && p) grow(*static_cast<RECT*>(p)); return out;
}
LRESULT CALLBACK proc(HWND hwnd,UINT msg,WPARAM wp,LPARAM lp) {
    LRESULT out=CallWindowProcW(previous,hwnd,msg,wp,lp);
    if(msg==WM_GETMINMAXINFO && lp) {
        auto* p=reinterpret_cast<MINMAXINFO*>(lp);
        p->ptMaxSize.x=p->ptMaxTrackSize.x=16384; p->ptMaxSize.y=p->ptMaxTrackSize.y=16384;
    }
    return out;
}
void desired(HWND hwnd,int& w,int& h) {
    RECT r{0,0,LONG(width),LONG(height)};
    AdjustWindowRectEx(&r,DWORD(GetWindowLongW(hwnd,GWL_STYLE)),FALSE,DWORD(GetWindowLongW(hwnd,GWL_EXSTYLE)));
    w=r.right-r.left; h=r.bottom-r.top;
}
HWND WINAPI create(DWORD ex,LPCWSTR cls,LPCWSTR title,DWORD style,int x,int y,int w,int h,
                   HWND parent,HMENU menu,HINSTANCE module,LPVOID arg) {
    HWND hwnd=createOriginal(ex,cls,title,style,x,y,w,h,parent,menu,module,arg);
    if(hwnd && !game && !parent && title && wcsstr(title,L"Batman") && (style & WS_CAPTION)) {
        game=hwnd;
        previous=reinterpret_cast<WNDPROC>(SetWindowLongPtrW(hwnd,GWLP_WNDPROC,reinterpret_cast<LONG_PTR>(proc)));
        desired(hwnd,w,h);
        SetWindowPos(hwnd,nullptr,0,0,w,h,SWP_NOZORDER|SWP_NOACTIVATE);
        BVR_LOG("[bo] game window requesting client %ux%u; verify actual scene at Present",width,height);
    }
    return hwnd;
}
BOOL WINAPI position(HWND hwnd,HWND after,int x,int y,int w,int h,UINT flags) {
    if(hwnd==game && !(flags & SWP_NOSIZE)) { desired(hwnd,w,h); x=y=0; }
    return positionOriginal(hwnd,after,x,y,w,h,flags);
}
void* replace_import(const char* name,void* replacement) {
    auto* base=reinterpret_cast<BYTE*>(GetModuleHandleW(nullptr));
    auto* dos=reinterpret_cast<IMAGE_DOS_HEADER*>(base);
    auto* nt=reinterpret_cast<IMAGE_NT_HEADERS32*>(base+dos->e_lfanew);
    auto rva=nt->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_IMPORT].VirtualAddress;
    if(!rva) return nullptr;
    for(auto* d=reinterpret_cast<IMAGE_IMPORT_DESCRIPTOR*>(base+rva);d->Name;++d) {
        if(_stricmp(reinterpret_cast<char*>(base+d->Name),"user32.dll") || !d->OriginalFirstThunk) continue;
        auto* names=reinterpret_cast<IMAGE_THUNK_DATA32*>(base+d->OriginalFirstThunk);
        auto* slots=reinterpret_cast<IMAGE_THUNK_DATA32*>(base+d->FirstThunk);
        for(;names->u1.AddressOfData;++names,++slots) {
            if(IMAGE_SNAP_BY_ORDINAL32(names->u1.Ordinal)) continue;
            auto* ibn=reinterpret_cast<IMAGE_IMPORT_BY_NAME*>(base+names->u1.AddressOfData);
            if(strcmp(reinterpret_cast<char*>(ibn->Name),name)) continue;
            DWORD protection;
            if(!VirtualProtect(&slots->u1.Function,4,PAGE_READWRITE,&protection)) return nullptr;
            void* original=reinterpret_cast<void*>(slots->u1.Function);
            InterlockedExchange(reinterpret_cast<LONG*>(&slots->u1.Function),LONG(replacement));
            DWORD ignored; VirtualProtect(&slots->u1.Function,4,protection,&ignored);
            return original;
        }
    }
    return nullptr;
}
}
void install() {
    const wchar_t* cmd=GetCommandLineW();
    auto* x=wcsstr(cmd,L"ResX="); auto* y=wcsstr(cmd,L"ResY=");
    if(!x || !y) return;
    width=wcstoul(x+5,nullptr,10); height=wcstoul(y+5,nullptr,10);
    if(width<512 || height<512 || width>8192 || height>8192) { width=height=0; return; }
    HMODULE user=GetModuleHandleW(L"user32.dll");
    if(MH_CreateHook(GetProcAddress(user,"CreateWindowExW"),reinterpret_cast<void*>(create),reinterpret_cast<void**>(&createOriginal))==MH_OK) {
        MH_QueueEnableHook(GetProcAddress(user,"CreateWindowExW")); MH_ApplyQueued();
    }
    metricsOriginal=reinterpret_cast<Metrics>(replace_import("GetSystemMetrics",reinterpret_cast<void*>(metrics)));
    monitorOriginal=reinterpret_cast<Monitor>(replace_import("GetMonitorInfoW",reinterpret_cast<void*>(monitor)));
    workOriginal=reinterpret_cast<Work>(replace_import("SystemParametersInfoW",reinterpret_cast<void*>(work)));
    positionOriginal=reinterpret_cast<Position>(replace_import("SetWindowPos",reinterpret_cast<void*>(position)));
}
}
