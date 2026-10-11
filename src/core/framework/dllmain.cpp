#include "framework.h"

#include "core/util/log.h"

#include <windows.h>
#include <atomic>
#include <string>
namespace bo { std::string HashFile(const std::wstring& path); }
namespace { std::atomic<DWORD> identity{0}; }
extern "C" __declspec(dllexport) DWORD WINAPI BO_Identity() { return identity.load(); }

namespace {

DWORD WINAPI InitThread(LPVOID) {
    wchar_t path[MAX_PATH]{};
    GetModuleFileNameW(nullptr,path,MAX_PATH);
    auto* name=wcsrchr(path,L'\\');
    if(_wcsicmp(name ? name+1 : path,L"BatmanOrigins.exe") != 0 ||
       bo::HashFile(path) != "1c373ed6edd3f5d42b097fcb2fc592811671b6f7d54b764b043b01e47871e5c2") {
        identity.store(2); return 0;
    }
    identity.store(1);
    bvr::framework::init();
    return 0;
}

} // namespace

BOOL WINAPI DllMain(HINSTANCE instance, DWORD reason, LPVOID) {
    if (reason == DLL_PROCESS_ATTACH) {
        DisableThreadLibraryCalls(instance);
        // All real work happens off the loader lock.
        HANDLE thread = CreateThread(nullptr, 0, InitThread, nullptr, 0, nullptr);
        if (thread) CloseHandle(thread);
    } else if (reason == DLL_PROCESS_DETACH) {
        // Breadcrumb that separates an ORDERLY shutdown from a hard kill. A log
        // that just stops mid-heartbeat with no line at all (session 23's
        // external report) means neither ran - i.e. TerminateProcess or a
        // fail-fast, not an exception our filter could ever have seen.
        BVR_LOG("shutdown: DLL_PROCESS_DETACH (orderly process exit)");
    }
    return TRUE;
}
