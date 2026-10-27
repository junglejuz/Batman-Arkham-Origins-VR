// FIRSTPERSON for Origins: Batman City's first-person mode (batman_city scene.cpp fp_update / fp_head_follow / fp_apply),
// re-found in Origins by code shape (memory origins-port-city-fixes-by-code-shape; RE_NOTES "FIRSTPERSON", STATIC
// 2026-10-20 on the retail exe):
//   APawn::GetPawnViewLocation 0x7ACD50 (City 0x7A6970, same instructions): Location +0x48, EyeHeight +0x2C0 (City +0x2A8).
//   APawn::GetViewRotation 0x7B4A80 (City 0x7BA650): Controller +0x1E4 (City +0x1CC).
//   The game's own head-bone read 0xF44E3B: FName ctor 0x485180 (L"Bip01_Head", 1, 1; ret 0xC), then
//   USkeletalMeshComponent::GetBoneLocation 0x84E590 (thiscall out, FName, space 0 = world; ret 0x10; City 0x850EE0) on
//   pawn +0x384 (= Pawn.Mesh, City +0x36C); its SkeletalMesh +0x1C4 (City +0x1A0).
// Every Pawn offset is City's + 0x18 except Location; the player -> controller link (City +0x30) and the controller ->
// pawn field are found LIVE by the pawn's back-pointer, as City finds the second.
#include "first_person.h"
#include "aim_depth.h"
#include "core/input/xinput_bridge.h"
#include "core/ui/overlay.h"
#include "core/util/log.h"
#include <windows.h>
#include <tlhelp32.h>
#include <imgui.h>
#include <atomic>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>

namespace bvr::bo::first_person {
namespace {
using W = uint32_t;
constexpr W kPlayerScanBegin = 0x20, kPlayerScanEnd = 0x80;   // ULocalPlayer -> PlayerController (City +0x30)
constexpr W kPCScanBegin = 0x100, kPCScanEnd = 0x700;         // PlayerController -> Pawn
constexpr W kPawnController = 0x1E4, kPawnLocation = 0x48, kPawnEyeHeight = 0x2C0, kPawnMesh = 0x384, kMeshSkeleton = 0x1C4;
constexpr W kFNameCtor = 0x485180, kGetBoneLocation = 0x84E590;
constexpr float kHeadCapUU = 140.0f; // ~1.5 m: anything further is not his head (City)
// HEADLOCK (JJ 2026-10-20: "when you start running or flying, the head moves ... the camera needs to be linked to the head
// movements so that the head doesn't intersect the camera at any point"): the smoothing may trail the real head by at most
// this much (~3 cm at 90 UU/m), far less than the 25 cm the camera sits in front of his face.
constexpr float kHeadMaxLagUU = 3.0f;
using FNameCtorFn = void*(__thiscall*)(void* name, const wchar_t* text, int findType, int unused);
using GetBoneLocationFn = float*(__thiscall*)(void* mesh, float* out, W nameIndex, W nameNumber, int space);

uintptr_t base = 0, imageEnd = 0;
wchar_t settings[MAX_PATH] = {};
std::atomic<bool> enabled{false}, followHead{true};
std::atomic<float> forwardCm{25.0f}, upCm{0.0f}, headSmoothS{0.08f};
// LEANBACK (JJ 2026-10-27 night: "When in first person mode and I move my physical head back my virtual camera is moving through Batman's
// head"): the headset's own movement is added on top of the eye point 25 cm in front of his face, so leaning back 25 cm puts the lens
// inside his head. The final eye may come no nearer than this to his head point along the look (level); sideways / up / down stay free.
std::atomic<float> leanMinCm{15.0f};
bool fpPair = false; // this pair is drawn in first person (apply reached its end)
std::atomic<unsigned> leanClamps{0};
// CAMPOS (JJ 2026-10-25: "put the adjustable camera from our Arkham Knight mod panel into the other 3 arkham mods"; AKVR
// TPOFFSET, akvr camera.cpp / hooks.cpp "camera position"): the third-person camera shifted left / right, down / up,
// back / forward, along the game camera's LEVEL heading and world up, so it never swings with the head. Gameplay only:
// not in first person, menus, or while the game camera is away from Batman (main menu stand-in, cutscenes elsewhere).
std::atomic<float> camRightCm{0.0f}, camUpCm{0.0f}, camFwdCm{0.0f};
std::atomic<unsigned> frames{0}, headFrames{0}, headMisses{0}, camFrames{0};
// HEADAIM (JJ 2026-10-27, Origins: "head aiming is not working in first-person mode"): in first person the headset's look
// direction becomes the direction gadgets aim. POVAIM (below) is how; the first try (Godoy301's Asylum PR: write Controller.Rotation
// +0x54) was confirmed and written every pair, LIVE 16:54 / 16:57, yet gadgets ignored it - the game copies its CAMERA into the
// controller.
struct Rot { int32_t pitch, yaw, roll; };
std::atomic<bool> lookAim{true};
// AIMTRIGGER (JJ 2026-10-27: "When you pull the left trigger to activate the batclaw or other weapons, it should activate the head
// aiming"; AKVR camera.cpp AIMTRIGGER, "aim with your head while holding the left trigger"): 1 = only while the left trigger is held
// (and 0.3 s after), 0 = always in first person.
std::atomic<int> aimMode{1};
std::atomic<ULONGLONG> ltLastMs{0};
bool left_trigger_held() {
    uint8_t lt = 0;
    input::last_composed_triggers(&lt, nullptr);
    const ULONGLONG now = GetTickCount64();
    if (lt > 30) ltLastMs.store(now, std::memory_order_relaxed);
    const ULONGLONG last = ltLastMs.load(std::memory_order_relaxed);
    return last && now - last < 300;
}
// LOOKFWD (JJ 2026-10-27: "I should not be able to spin the camera around to look directly at batman's face up close while in first
// person mode"): the lens sits "camera in front of his face" out from his head in the direction you LOOK (game camera x headset,
// level), decided once per pair, so his head is always behind the lens whichever way the stick or your head turns. FACEFWD (his
// facing) put the lens in front of his face and let you turn round to it.
float pairPush[2] = {1.0f, 0.0f}; bool pushValid = false;
float pairCamB[3][3] = {}; float pairLook[3] = {}; bool pairLookValid = false; float pairOrigin[3] = {};
W playerOff = 0, pawnOff = 0;               // found live
float eye[3] = {};
bool eyeValid = false;                      // this pair's eye point (game thread)
bool camValid = false;                      // CAMPOS: this pair's Batman found (eye[] = his eye point, no head follow)
float headOff[3] = {};
bool headOffValid = false;
LARGE_INTEGER headLastT = {};
W headName[2] = {};
bool headNameReady = false;
const char* gateWhy = "";

bool rd(uintptr_t p, void* out, size_t n) {
    __try { memcpy(out, reinterpret_cast<const void*>(p), n); return true; }
    __except (EXCEPTION_EXECUTE_HANDLER) { return false; }
}
uintptr_t word(uintptr_t p) { W v = 0; return rd(p, &v, 4) ? v : 0; }
bool object(uintptr_t p) { // a C++ object of the game (vtable in its image)
    if (p < 0x10000 || (p & 3)) return false;
    const uintptr_t vt = word(p);
    return vt >= base && vt < imageEnd;
}
bool finite3(const float* v) { return std::isfinite(v[0]) && std::isfinite(v[1]) && std::isfinite(v[2]); }
void gate(const char* why) {
    if (!why) why = "";
    if (why == gateWhy) return;
    gateWhy = why;
    BVR_LOG("[bo] first person %s%s", *why ? "paused: " : "", *why ? why : "active again");
}
// --- HEADAIM helpers (UE3 FRotator: 65536 units per turn; X forward, Y right, Z up) -----------------------------------
constexpr float kPi = 3.14159265358979f;
int32_t wrapU(int64_t n) { const uint32_t u = uint32_t(n) & 65535u; return u >= 32768 ? int32_t(u) - 65536 : int32_t(u); }
float radU(int32_t n) { return float(wrapU(n)) * (2 * kPi / 65536); }
int32_t unitsR(float r) { return wrapU(int64_t(std::lround(r * (65536 / (2 * kPi))))); }
// AIMSTATE (JJ 2026-10-27 evening: "the bat claw is still not firing towards where I am looking. It is firing in the direction of
// Batman's body"). LIVE 17:55: writing the head's look into the camera's view record (POVAIM) only moved the reticle; writing it into
// Controller.Rotation made the game camera follow it ("the controller FEEDS the camera") = the controller is the camera's OWN state
// (classic UE3: the stick turns Controller.Rotation, the camera and Batman's aiming body follow it). So while aiming, the controller is
// steered to the head's look, like Godoy301's Asylum head aim: each pair, body = controller - our last contribution (stick turns
// kept), body pitch = the pitch when the trigger went down (the stick's up / down is the head's while aiming, never drifts - Asylum
// AIMLEVEL), controller = body x head; both eyes render from the body basis, so the head is not applied twice. On release the body
// goes back into the controller. The view record write and its finder are gone (the camera now follows the controller by itself).
// The headset rotation as scene.cpp delta() builds it (OpenXR quaternion, z flipped), 3x3 part only.
void head_rot(const float q[4], float d[3][3]) {
    const float x = q[0], y = q[1], z = q[2], w = q[3];
    const float R[3][3] = {{1 - 2 * (y * y + z * z), 2 * (x * y - z * w), 2 * (x * z + y * w)},
                           {2 * (x * y + z * w), 1 - 2 * (x * x + z * z), 2 * (y * z - x * w)},
                           {2 * (x * z - y * w), 2 * (y * z + x * w), 1 - 2 * (x * x + y * y)}};
    const float flip[3] = {1, 1, -1};
    for (int i = 0; i < 3; ++i) for (int j = 0; j < 3; ++j) d[i][j] = R[i][j] * flip[i] * flip[j];
}
// Body basis as view-matrix columns (right, up, forward), row-vector convention as the scene view (Asylum head_aim body_view).
void basis(const Rot& b, float m[3][3]) {
    const float p = radU(b.pitch), y = radU(b.yaw), cp = cosf(p), sp = sinf(p), cy = cosf(y), sy = sinf(y);
    m[0][0] = -sy;      m[1][0] = cy;       m[2][0] = 0;
    m[0][1] = -sp * cy; m[1][1] = -sp * sy; m[2][1] = cp;
    m[0][2] = cp * cy;  m[1][2] = cp * sy;  m[2][2] = sp;
}
uintptr_t aimPawnNow = 0; // this pair's pawn (update)
constexpr W kCtrlRotation = 0x54; // Actor.Rotation (Location +0x48 + 0xC); AIMCHECK 16:54 / 16:57 confirmed it on the controller
std::atomic<bool> aimWatch{false}; // AIMWATCH, settings.ini [first_person] aim_watch=1 (a diagnostic: off for players since 0.1.5)
std::atomic<unsigned> aimWrites{0}, aimFailures{0};
Rot look_rot(const float f[3]) {
    Rot r{};
    r.yaw = unitsR(atan2f(f[1], f[0]));
    r.pitch = unitsR(atan2f(f[2], sqrtf(f[0] * f[0] + f[1] * f[1])));
    return r;
}
bool read_rot(uintptr_t at, Rot& r) { return rd(at, &r, sizeof r); }
bool write_rot(uintptr_t at, const Rot& r) {
    __try { memcpy(reinterpret_cast<void*>(at), &r, sizeof r); return true; }
    __except (EXCEPTION_EXECUTE_HANDLER) { return false; }
}
bool stActive = false; uintptr_t stPc = 0; int32_t stYawOffset = 0, stPitch = 0; // our yaw contribution, the body's pitch
Rot stTarget{};                                                                    // this pair's look written into the controller
// AIMDEPTH (JJ 2026-10-27 night: the batclaw "is still offset from the reticle to up and right of the actual reticle"; AIMPOV - our
// eye point written into the camera's view records - changed nothing, LIVE 19:10): the game aims from ITS camera, over his right
// shoulder, so a look parallel to ours lands above / right. The controller is turned so the game camera's ray passes through the point
// we look at (the left eye's depth under the head's aim, aim_depth.cpp); no point (sky, too near, not read yet) = parallel as before.
float pairGameO[3] = {}; Rot pairGameLook{};       // this pair's game camera, before our changes (first eye)
std::atomic<unsigned> depthAims{0}, plainAims{0};
// AIM3P (JJ 2026-10-27 night: "make the head aiming work in third person the same way it does with first person"): the same controller
// steering while the left trigger is held. In third person the game camera ORBITS Batman with the controller, so steered by the head it
// would swing round him: each pair is drawn from where the camera would be for the stick's own direction - the game camera's offset from
// his eye point, taken out of the camera's actual rotation (it may lag) and laid along the body's - and after the trigger is let go the
// same until the camera has caught up with the stick again (<= 1.5 s), so it never swings back.
std::atomic<bool> lookAim3p{true};
float tpBody[3][3] = {}; float tpPos[3] = {}; bool tpComp = false; ULONGLONG tpCompUntil = 0;
std::atomic<unsigned> tpPairs{0};
bool pairAimed = false; float pairBody[3][3] = {};                                // this pair's body basis (both eyes)
void aim_watch_start(uintptr_t pc); // AIMWATCH (below)
void state_release(const char* why) {
    if (!stActive) return;
    Rot cur{};
    bool ok = object(stPc) && read_rot(stPc + kCtrlRotation, cur);
    if (ok) { cur.yaw = wrapU(int64_t(cur.yaw) - stYawOffset); cur.pitch = stPitch; ok = write_rot(stPc + kCtrlRotation, cur); }
    if (!ok) aimFailures.fetch_add(1, std::memory_order_relaxed);
    BVR_LOG("[bo] head aim released (%s): the stick's own direction is back in the controller%s", why, ok ? "" : " - FAILED");
    stActive = false; stPc = 0; stYawOffset = 0;
}
// First eye of a pair: steer the controller to the head's look; true = this pair renders from pairBody.
bool state_drive(uintptr_t pc, const float headQ[4]) {
    if (stActive && stPc != pc) state_release("controller changed");
    for (int i = 0; i < 4; ++i) if (!std::isfinite(headQ[i])) return false;
    Rot cur{};
    if (!read_rot(pc + kCtrlRotation, cur)) { state_release("controller unreadable"); return false; }
    Rot body = cur;
    if (stActive) body.yaw = wrapU(int64_t(cur.yaw) - stYawOffset); // stick turns since the last pair stay
    else stPitch = wrapU(cur.pitch);
    body.pitch = stPitch;
    body.roll = 0;
    float B[3][3], D[3][3], f[3] = {};
    basis(body, B); head_rot(headQ, D);
    for (int i = 0; i < 3; ++i) for (int k = 0; k < 3; ++k) f[i] += B[i][k] * D[k][2]; // forward = column 2 of B x D
    Rot look = look_rot(f);
    float P[3];
    if (aim_depth::point(P)) { // AIMDEPTH: from the game camera through the looked-at point
        const float d[3] = {P[0] - pairGameO[0], P[1] - pairGameO[1], P[2] - pairGameO[2]};
        const float len = sqrtf(d[0] * d[0] + d[1] * d[1] + d[2] * d[2]);
        if (len > 100.0f && d[0] * f[0] + d[1] * f[1] + d[2] * f[2] > 0.9f * len) { look = look_rot(d); depthAims.fetch_add(1, std::memory_order_relaxed); }
        else plainAims.fetch_add(1, std::memory_order_relaxed);
    } else plainAims.fetch_add(1, std::memory_order_relaxed);
    Rot target = cur; target.yaw = look.yaw; target.pitch = look.pitch;
    Rot back{};
    if (!write_rot(pc + kCtrlRotation, target) || !read_rot(pc + kCtrlRotation, back) || back.yaw != target.yaw || back.pitch != target.pitch) {
        aimFailures.fetch_add(1, std::memory_order_relaxed);
        write_rot(pc + kCtrlRotation, cur);
        state_release("write failed");
        return false;
    }
    if (!stActive) BVR_LOG("[bo] head aim active: Batman's controller follows your look (controller %08X, body pitch %d)", unsigned(pc), stPitch);
    stYawOffset = wrapU(int64_t(target.yaw) - body.yaw);
    stTarget = target;
    stActive = true; stPc = pc;
    memcpy(pairBody, B, sizeof B);
    aimWrites.fetch_add(1, std::memory_order_relaxed);
    aim_watch_start(pc);
    return true;
}
// AIMWATCH: the first time head aim writes in a launch, a 5 s hardware read / write watch (debug registers, every thread - memory
// hw-read-watch-finds-readers, SWBF2 probe.cpp) on Controller.Rotation and Pawn.Rotation (yaw, pitch) logs every
// distinct game instruction that touches them ("[bo] AIMWATCH ..."), so the code that aims a gadget can be named if it still flies
// straight. No effect on the game beyond a trap per access for those 5 s.
constexpr int kWatchSites = 96;
struct WatchSite { uint32_t eip, ret; uint8_t dr; std::atomic<uint32_t> n; };
WatchSite watchSites[kWatchSites];
std::atomic<int> watchN{0};
uintptr_t watchAddr[4] = {};
const char* const kWatchName[4] = {"Controller.Rotation yaw", "Pawn.Rotation yaw", "Controller.Rotation pitch", "Pawn.Rotation pitch"};
std::atomic<bool> watchDone{false};
LONG CALLBACK watch_handler(EXCEPTION_POINTERS* ep) {
    if (ep->ExceptionRecord->ExceptionCode != EXCEPTION_SINGLE_STEP) return EXCEPTION_CONTINUE_SEARCH;
    CONTEXT& c = *ep->ContextRecord;
    const DWORD hit = c.Dr6 & 0xF;
    if (!hit) return EXCEPTION_CONTINUE_SEARCH;
    c.Dr6 = 0;
    uint8_t dr = 0; while (!(hit & (1u << dr))) ++dr;
    uint32_t ret = 0; rd(c.Esp, &ret, 4); // never read the watched values here (that would trap again)
    const int n = watchN.load(std::memory_order_acquire) < kWatchSites ? watchN.load(std::memory_order_acquire) : kWatchSites;
    for (int i = 0; i < n; ++i)
        if (watchSites[i].eip == c.Eip && watchSites[i].dr == dr) { watchSites[i].n.fetch_add(1, std::memory_order_relaxed); return EXCEPTION_CONTINUE_EXECUTION; }
    const int k = watchN.fetch_add(1, std::memory_order_acq_rel);
    if (k < kWatchSites) { watchSites[k].eip = c.Eip; watchSites[k].ret = ret; watchSites[k].dr = dr; watchSites[k].n.store(1); }
    return EXCEPTION_CONTINUE_EXECUTION;
}
int watch_arm(bool on) { // every thread of the game but this one; Dr0-3 = 4 bytes, read / write
    const DWORD self = GetCurrentThreadId(), pid = GetCurrentProcessId();
    HANDLE snap = CreateToolhelp32Snapshot(TH32CS_SNAPTHREAD, 0);
    if (snap == INVALID_HANDLE_VALUE) return 0;
    THREADENTRY32 te{sizeof te};
    int n = 0;
    for (BOOL ok = Thread32First(snap, &te); ok; ok = Thread32Next(snap, &te)) {
        if (te.th32OwnerProcessID != pid || te.th32ThreadID == self) continue;
        HANDLE t = OpenThread(THREAD_GET_CONTEXT | THREAD_SET_CONTEXT | THREAD_SUSPEND_RESUME, FALSE, te.th32ThreadID);
        if (!t) continue;
        if (SuspendThread(t) != DWORD(-1)) {
            CONTEXT c{}; c.ContextFlags = CONTEXT_DEBUG_REGISTERS;
            if (GetThreadContext(t, &c)) {
                DWORD dr7 = c.Dr7 & ~0xFFFF00FFu;
                if (on) {
                    c.Dr0 = DWORD(watchAddr[0]); c.Dr1 = DWORD(watchAddr[1]); c.Dr2 = DWORD(watchAddr[2]); c.Dr3 = DWORD(watchAddr[3]);
                    for (int i = 0; i < 4; ++i) if (watchAddr[i]) dr7 |= (1u << (2 * i)) | (0xFu << (16 + 4 * i)); // L, RW=11, LEN=11
                } else { c.Dr0 = c.Dr1 = c.Dr2 = c.Dr3 = 0; }
                c.Dr7 = dr7;
                if (SetThreadContext(t, &c)) ++n;
            }
            ResumeThread(t);
        }
        CloseHandle(t);
    }
    CloseHandle(snap);
    return n;
}
DWORD WINAPI watch_thread(void*) {
    static PVOID h = AddVectoredExceptionHandler(1, watch_handler);
    if (!h) { BVR_LOG("[bo] AIMWATCH: no exception handler - skipped"); return 0; }
    BVR_LOG("[bo] AIMWATCH armed on %d threads for 5 s: %08X %08X %08X %08X (%s / %s / %s / %s)", watch_arm(true), unsigned(watchAddr[0]),
            unsigned(watchAddr[1]), unsigned(watchAddr[2]), unsigned(watchAddr[3]), kWatchName[0], kWatchName[1], kWatchName[2], kWatchName[3]);
    Sleep(5000);
    const int threads = watch_arm(false);
    Sleep(50);
    const int n = watchN.load() < kWatchSites ? watchN.load() : kWatchSites;
    HMODULE self = nullptr;
    GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT, reinterpret_cast<LPCWSTR>(&watch_thread), &self);
    BVR_LOG("[bo] AIMWATCH done (disarmed on %d threads): %d distinct instructions", threads, n);
    for (int i = 0; i < n; ++i) {
        const uint32_t e = watchSites[i].eip;
        HMODULE m = nullptr;
        GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT, reinterpret_cast<LPCWSTR>(uintptr_t(e)), &m);
        BVR_LOG("[bo] AIMWATCH %s: after %08X (%s+0x%X) x%u, [esp] %08X", kWatchName[watchSites[i].dr], e,
                m == self ? "ours" : uintptr_t(m) == base ? "exe" : "other", m ? unsigned(e - uintptr_t(m)) : e,
                watchSites[i].n.load(), watchSites[i].ret);
    }
    watchDone.store(true);
    return 0;
}
void aim_watch_start(uintptr_t pc) {
    static bool started = false;
    if (started || !aimWatch.load(std::memory_order_relaxed)) return;
    started = true;
    watchAddr[0] = pc + kCtrlRotation + 4;
    watchAddr[1] = aimPawnNow ? aimPawnNow + kCtrlRotation + 4 : 0;
    watchAddr[2] = pc + kCtrlRotation;
    watchAddr[3] = aimPawnNow ? aimPawnNow + kCtrlRotation : 0;
    if (HANDLE t = CreateThread(nullptr, 0, watch_thread, nullptr, 0, nullptr)) CloseHandle(t);
}
// AIMFIND (JJ 2026-10-27, Origins head aim "still not working" although Controller.Rotation +0x54 was confirmed and written every
// pair = the game's aim / camera keeps its OWN rotation and copies it into the controller; AKVR AIMHEAD writes the camera's
// finalize block instead): a 20 s recording, 10 samples a second, of the controller and every game object it points to (0xC00
// bytes each) with the game camera's yaw / pitch, so tools/aimfinder.py can find the fields that carry the camera rotation.
constexpr W kFindBytes = 0xC00;
constexpr int kFindMaxObjs = 64;
std::atomic<ULONGLONG> findUntil{0};
FILE* findBin = nullptr;
uintptr_t findObj[kFindMaxObjs] = {}; W findField[kFindMaxObjs] = {}; int findN = 0;
ULONGLONG findNext = 0; unsigned findSamples = 0;
uintptr_t lastPlayer = 0;
void find_sample(const float view[4][4]) {
    const ULONGLONG now = GetTickCount64(), until = findUntil.load();
    if (!until) return;
    if (now >= until) {
        if (findBin) { fclose(findBin); findBin = nullptr; }
        findUntil.store(0);
        BVR_LOG("[bo] AIMFIND done: %u samples of %d objects", findSamples, findN);
        return;
    }
    if (now < findNext) return;
    findNext = now + 100;
    const uintptr_t pc = lastPlayer ? word(lastPlayer + (playerOff ? playerOff : 0x30)) : 0;
    if (!object(pc)) return;
    if (!findBin) { // first sample: the object list
        wchar_t dir[MAX_PATH], path[MAX_PATH];
        const wchar_t* slash = wcsrchr(settings, L'\\');
        swprintf_s(dir, L"%.*s\\captures", int(slash ? slash - settings : 0), settings);
        CreateDirectoryW(dir, nullptr);
        swprintf_s(path, L"%s\\aimfinder_%llu.bin", dir, now);
        if (_wfopen_s(&findBin, path, L"wb") || !findBin) { findUntil.store(0); BVR_LOG("[bo] AIMFIND: cannot write %ls", path); return; }
        findN = 0; findSamples = 0;
        findObj[findN] = pc; findField[findN++] = 0xFFFFFFFF; // the controller itself
        for (W off = 0; off < kFindBytes && findN < kFindMaxObjs; off += 4) {
            const uintptr_t q = word(pc + off);
            bool dup = false; for (int i = 0; i < findN; ++i) dup |= findObj[i] == q;
            if (!dup && q != pc && object(q)) { findObj[findN] = q; findField[findN++] = off; }
        }
        const W magic = 0x46444D41, n = W(findN), bytes = kFindBytes; // "AMDF"
        fwrite(&magic, 4, 1, findBin); fwrite(&n, 4, 1, findBin); fwrite(&bytes, 4, 1, findBin);
        for (int i = 0; i < findN; ++i) { const W p = W(findObj[i]); fwrite(&findField[i], 4, 1, findBin); fwrite(&p, 4, 1, findBin); }
        BVR_LOG("[bo] AIMFIND recording %d objects (the controller %08X + the objects it points to) -> %ls", findN, unsigned(pc), path);
    }
    const float yaw = atan2f(view[1][2], view[0][2]) * 180 / kPi;
    const float pitch = asinf(fmaxf(-1.f, fminf(1.f, view[2][2]))) * 180 / kPi;
    const float t = float(now % 100000000ULL) / 1000.0f;
    fwrite(&t, 4, 1, findBin); fwrite(&yaw, 4, 1, findBin); fwrite(&pitch, 4, 1, findBin);
    static uint8_t buf[kFindBytes];
    for (int i = 0; i < findN; ++i) { if (!rd(findObj[i], buf, kFindBytes)) memset(buf, 0, kFindBytes); fwrite(buf, 1, kFindBytes, findBin); }
    ++findSamples;
}
uintptr_t pawn_of(uintptr_t pc, W off) {
    const uintptr_t q = word(pc + off);
    if (q == pc || !object(q) || word(q + kPawnController) != pc) return 0;
    float eh = 0;
    return rd(q + kPawnEyeHeight, &eh, 4) && std::isfinite(eh) && eh > -50.0f && eh < 300.0f ? q : 0;
}
uintptr_t find_pawn(uintptr_t player) {
    if (playerOff && pawnOff) {
        const uintptr_t pc = word(player + playerOff);
        if (object(pc)) if (const uintptr_t pawn = pawn_of(pc, pawnOff)) return pawn;
    }
    static ULONGLONG s_lastScan = 0; // no pawn (menus, some cutscenes): look again twice a second
    const ULONGLONG now = GetTickCount64();
    if (now - s_lastScan < 500) return 0;
    s_lastScan = now;
    for (W lo = kPlayerScanBegin; lo < kPlayerScanEnd; lo += 4) {
        const uintptr_t pc = word(player + lo);
        if (!object(pc)) continue;
        for (W po = kPCScanBegin; po < kPCScanEnd; po += 4)
            if (const uintptr_t pawn = pawn_of(pc, po)) {
                if (lo != playerOff || po != pawnOff)
                    BVR_LOG("[bo] first person: player +0x%X = controller %08X, its +0x%X = Batman (pawn %08X)", lo,
                            unsigned(pc), po, unsigned(pawn));
                playerOff = lo; pawnOff = po;
                return pawn;
            }
    }
    return 0;
}
bool head_name() {
    if (headNameReady) return true;
    __try { reinterpret_cast<FNameCtorFn>(base + kFNameCtor - 0x400000)(headName, L"Bip01_Head", 1, 1); }
    __except (EXCEPTION_EXECUTE_HANDLER) { return false; }
    headNameReady = true;
    BVR_LOG("[bo] first person head: name Bip01_Head = %u/%u", headName[0], headName[1]);
    return true;
}
bool bone(uintptr_t mesh, float* out) {
    __try { reinterpret_cast<GetBoneLocationFn>(base + kGetBoneLocation - 0x400000)(reinterpret_cast<void*>(mesh), out, headName[0], headName[1], 0); }
    __except (EXCEPTION_EXECUTE_HANDLER) { return false; }
    return true;
}
// HEADCAM (City): the eye point follows his head bone, smoothed (real time, so both eyes of a pair share it) and capped.
void follow_head(uintptr_t pawn) {
    if (!followHead.load(std::memory_order_relaxed)) { headOffValid = false; return; }
    auto miss = [](const char* why) {
        headOffValid = false;
        if (headMisses.fetch_add(1, std::memory_order_relaxed) < 6) BVR_LOG("[bo] first person head: %s - steady eye point", why);
    };
    const uintptr_t mesh = word(pawn + kPawnMesh);
    if (!object(mesh)) return miss("no mesh at pawn +0x384");
    if (!object(word(mesh + kMeshSkeleton))) return miss("mesh has no skeleton");
    if (!head_name()) return miss("Bip01_Head name failed");
    alignas(16) float head[4] = {};
    if (!bone(mesh, head)) return miss("bone read faulted");
    if (!finite3(head) || (head[0] == 0.0f && head[1] == 0.0f && head[2] == 0.0f)) return miss("no Bip01_Head bone");
    const float d[3] = {head[0] - eye[0], head[1] - eye[1], head[2] - eye[2]};
    if (fabsf(d[0]) > kHeadCapUU || fabsf(d[1]) > kHeadCapUU || fabsf(d[2]) > kHeadCapUU) {
        char b[96];
        snprintf(b, sizeof(b), "head %.0f, %.0f, %.0f UU from the eye point (cap %.0f)", d[0], d[1], d[2], kHeadCapUU);
        return miss(b);
    }
    LARGE_INTEGER now, f;
    QueryPerformanceCounter(&now);
    QueryPerformanceFrequency(&f);
    const float dt = headLastT.QuadPart ? float(double(now.QuadPart - headLastT.QuadPart) / double(f.QuadPart)) : 0.0f;
    headLastT = now;
    const float tau = headSmoothS.load(std::memory_order_relaxed);
    const float a = !headOffValid || tau <= 0.001f ? 1.0f : 1.0f - expf(-(dt > 0.1f ? 0.1f : dt) / tau);
    float lag[3], lag2 = 0.0f;
    for (int i = 0; i < 3; ++i) { headOff[i] += (d[i] - headOff[i]) * a; lag[i] = d[i] - headOff[i]; lag2 += lag[i] * lag[i]; }
    if (lag2 > kHeadMaxLagUU * kHeadMaxLagUU) { // HEADLOCK: pull the smoothed point up to the real head
        const float k = 1.0f - kHeadMaxLagUU / sqrtf(lag2);
        for (int i = 0; i < 3; ++i) headOff[i] += lag[i] * k;
    }
    for (int i = 0; i < 3; ++i) eye[i] += headOff[i];
    headOffValid = true;
    if (headFrames.fetch_add(1, std::memory_order_relaxed) == 0)
        BVR_LOG("[bo] first person head: mesh %08X, Bip01_Head - eye point = %.1f, %.1f, %.1f UU", unsigned(mesh), d[0], d[1], d[2]);
}
void save() {
    if (!settings[0]) return;
    wchar_t v[32];
    WritePrivateProfileStringW(L"first_person", L"on", enabled.load() ? L"1" : L"0", settings);
    swprintf_s(v, L"%.1f", forwardCm.load()); WritePrivateProfileStringW(L"first_person", L"forward_cm", v, settings);
    swprintf_s(v, L"%.1f", upCm.load()); WritePrivateProfileStringW(L"first_person", L"up_cm", v, settings);
    swprintf_s(v, L"%.1f", leanMinCm.load()); WritePrivateProfileStringW(L"first_person", L"lean_min_cm", v, settings);
    WritePrivateProfileStringW(L"first_person", L"head", followHead.load() ? L"1" : L"0", settings);
    WritePrivateProfileStringW(L"first_person", L"look_aim", lookAim.load() ? L"1" : L"0", settings);
    WritePrivateProfileStringW(L"first_person", L"look_aim_3p", lookAim3p.load() ? L"1" : L"0", settings);
    WritePrivateProfileStringW(L"first_person", L"look_aim_trigger", aimMode.load() == 1 ? L"1" : L"0", settings);
    swprintf_s(v, L"%.3f", headSmoothS.load()); WritePrivateProfileStringW(L"first_person", L"head_smooth", v, settings);
    swprintf_s(v, L"%.1f", camRightCm.load()); WritePrivateProfileStringW(L"camera_position", L"right_cm", v, settings);
    swprintf_s(v, L"%.1f", camUpCm.load()); WritePrivateProfileStringW(L"camera_position", L"up_cm", v, settings);
    swprintf_s(v, L"%.1f", camFwdCm.load()); WritePrivateProfileStringW(L"camera_position", L"forward_cm", v, settings);
}
bool cam_offset_set() {
    return camRightCm.load(std::memory_order_relaxed) != 0.0f || camUpCm.load(std::memory_order_relaxed) != 0.0f ||
           camFwdCm.load(std::memory_order_relaxed) != 0.0f;
}
// CAMPOS: the camera origin moved along the level right / level forward of the game camera and world up.
void cam_offset(float view[4][4], const float* o, float unitsPerMetre) {
    float rx = view[0][0], ry = view[1][0];
    float len = sqrtf(rx * rx + ry * ry);
    if (len < 1e-3f) return; // rolled on its side: no level heading
    rx /= len; ry /= len;
    float fx = view[0][2], fy = view[1][2];
    len = sqrtf(fx * fx + fy * fy);
    if (len > 1e-3f) { fx /= len; fy /= len; } else { fx = ry; fy = -rx; } // straight down / up: forward from right
    const float cm = unitsPerMetre * 0.01f, r = camRightCm.load(std::memory_order_relaxed) * cm,
                f = camFwdCm.load(std::memory_order_relaxed) * cm, u = camUpCm.load(std::memory_order_relaxed) * cm;
    const float n[3] = {o[0] + rx * r + fx * f, o[1] + ry * r + fy * f, o[2] + u};
    for (int j = 0; j < 3; ++j) view[3][j] = -(n[0] * view[0][j] + n[1] * view[1][j] + n[2] * view[2][j]);
    camFrames.fetch_add(1, std::memory_order_relaxed);
}
float get(const wchar_t* key, float fallback, float lo, float hi, const wchar_t* section = L"first_person") {
    wchar_t t[32] = {};
    GetPrivateProfileStringW(section, key, L"", t, 32, settings);
    if (!t[0]) return fallback;
    const float v = float(wcstod(t, nullptr));
    return std::isfinite(v) && v >= lo && v <= hi ? v : fallback;
}
} // namespace

void install(uintptr_t imageBase, const wchar_t* settingsPath) {
    base = imageBase;
    const auto* dos = reinterpret_cast<const IMAGE_DOS_HEADER*>(imageBase);
    const auto* nt = reinterpret_cast<const IMAGE_NT_HEADERS*>(imageBase + dos->e_lfanew);
    imageEnd = imageBase + nt->OptionalHeader.SizeOfImage;
    wcscpy_s(settings, settingsPath);
    enabled.store(GetPrivateProfileIntW(L"first_person", L"on", 0, settings) != 0);
    forwardCm.store(get(L"forward_cm", 25.0f, -20.0f, 80.0f));
    upCm.store(get(L"up_cm", 0.0f, -40.0f, 40.0f));
    leanMinCm.store(get(L"lean_min_cm", 15.0f, 0.0f, 40.0f));
    followHead.store(GetPrivateProfileIntW(L"first_person", L"head", 1, settings) != 0);
    lookAim.store(GetPrivateProfileIntW(L"first_person", L"look_aim", 1, settings) != 0);
    lookAim3p.store(GetPrivateProfileIntW(L"first_person", L"look_aim_3p", 1, settings) != 0);
    aimWatch.store(GetPrivateProfileIntW(L"first_person", L"aim_watch", 0, settings) != 0);
    aimMode.store(GetPrivateProfileIntW(L"first_person", L"look_aim_trigger", 1, settings) != 0 ? 1 : 0);
    headSmoothS.store(get(L"head_smooth", 0.08f, 0.0f, 0.5f));
    camRightCm.store(get(L"right_cm", 0.0f, -300.0f, 300.0f, L"camera_position"));
    camUpCm.store(get(L"up_cm", 0.0f, -300.0f, 300.0f, L"camera_position"));
    camFwdCm.store(get(L"forward_cm", 0.0f, -300.0f, 300.0f, L"camera_position"));
    overlay::set_chord_double_tap(&toggle);
    BVR_LOG("[bo] first person %s (both sticks clicked twice quickly toggles it); camera position right %.0f, up %.0f, "
            "forward %.0f cm", enabled.load() ? "ON" : "off", camRightCm.load(), camUpCm.load(), camFwdCm.load());
}

void update(void* localPlayer, const char* blocked) {
    eyeValid = false;
    camValid = false;
    aimPawnNow = 0;
    if (localPlayer) lastPlayer = reinterpret_cast<uintptr_t>(localPlayer); // AIMFIND
    const bool fp = enabled.load(std::memory_order_relaxed);
    const bool aim3p = !fp && lookAim.load(std::memory_order_relaxed) && lookAim3p.load(std::memory_order_relaxed);
    if (!fp && !aim3p) state_release("third-person head aim off");
    if ((!fp && !cam_offset_set() && !aim3p) || !localPlayer) return;
    // FPGATE (City, JJ 2026-10-11: "must always be disabled when going into main menu ... or any other loading screen and
    // menus"): the setting stays on, the game's own camera is used while a menu is up. CAMPOS too.
    if (blocked) { state_release(blocked); tpCompUntil = 0; if (fp) gate(blocked); return; }
    const uintptr_t pawn = find_pawn(reinterpret_cast<uintptr_t>(localPlayer));
    if (!pawn) { state_release("no Batman"); tpCompUntil = 0; if (fp) gate("no Batman to follow (menu or cutscene)"); return; }
    aimPawnNow = pawn;
    float loc[3] = {}, eh = 0;
    if (!rd(pawn + kPawnLocation, loc, 12) || !rd(pawn + kPawnEyeHeight, &eh, 4) || !finite3(loc) || !std::isfinite(eh)) return;
    eye[0] = loc[0]; eye[1] = loc[1]; eye[2] = loc[2] + eh; // APawn::GetPawnViewLocation
    if (!fp) { camValid = true; return; } // CAMPOS: only the near-Batman check below needs him
    follow_head(pawn);
    eyeValid = true;
    static bool s_logged = false;
    if (!s_logged) { s_logged = true; BVR_LOG("[bo] first person: eye (%.1f, %.1f, %.1f) = location + eye height %.1f", eye[0], eye[1], eye[2], eh); }
}

// The view matrix: row vectors, v_view = (v - origin) * R, translation row = -origin * R; columns of R: right, up, forward.
void apply(float view[4][4], float unitsPerMetre, const float headQ[4], bool firstEye, bool lastEye) {
    (void)lastEye;
    if (firstEye) fpPair = false;
    if (firstEye && findUntil.load(std::memory_order_relaxed)) find_sample(view); // AIMFIND: the game camera, before any change
    if (!eyeValid && !camValid) { if (firstEye) state_release("first person paused"); return; }
    float o[3];
    for (int i = 0; i < 3; ++i) o[i] = -(view[3][0] * view[i][0] + view[3][1] * view[i][1] + view[3][2] * view[i][2]);
    const float dx = eye[0] - o[0], dy = eye[1] - o[1], dz = eye[2] - o[2];
    // FPGATE (City): the main menu has a stand-in pawn far from the menu scene's camera; third person sits ~2-4 m behind him.
    if (!(sqrtf(dx * dx + dy * dy + dz * dz) < 12.0f * unitsPerMetre)) {
        state_release("game camera far from Batman"); tpCompUntil = 0;
        if (eyeValid) gate("game camera far from Batman");
        return;
    }
    if (camValid) { // third person: AIM3P, then CAMPOS
        const bool headOk3 = headQ && std::isfinite(headQ[0]) && std::isfinite(headQ[1]) && std::isfinite(headQ[2]) && std::isfinite(headQ[3]);
        if (firstEye) {
            const float g[3] = {view[0][2], view[1][2], view[2][2]};
            pairGameLook = look_rot(g);
            for (int i = 0; i < 3; ++i) pairGameO[i] = o[i]; // AIMDEPTH: the game's own camera, the start of its aim
            pairAimed = false;
            const uintptr_t pc = aimPawnNow ? word(aimPawnNow + kPawnController) : 0;
            const bool want = lookAim.load(std::memory_order_relaxed) && lookAim3p.load(std::memory_order_relaxed) && headOk3 &&
                              object(pc) && left_trigger_held(); // third person: only while aiming, always
            if (want) pairAimed = state_drive(pc, headQ);
            else state_release("left trigger released");
            const ULONGLONG now = GetTickCount64();
            tpComp = false;
            if (pairAimed) { memcpy(tpBody, pairBody, sizeof tpBody); tpCompUntil = now + 1500; tpComp = true; }
            else if (tpCompUntil && now < tpCompUntil && object(pc)) { // the camera catching up with the stick after the release
                Rot cr{};
                if (read_rot(pc + kCtrlRotation, cr)) {
                    cr.roll = 0; basis(cr, tpBody);
                    const float dotF = view[0][2] * tpBody[0][2] + view[1][2] * tpBody[1][2] + view[2][2] * tpBody[2][2];
                    if (dotF > 0.99996f) tpCompUntil = 0; // within ~0.5 deg: the game camera is the stick's again
                    else tpComp = true;
                }
            } else tpCompUntil = 0;
            if (tpComp) { // where the camera would be for the body's direction: its offset in its own frame, laid along the body
                float loc[3];
                for (int j = 0; j < 3; ++j) loc[j] = (o[0] - eye[0]) * view[0][j] + (o[1] - eye[1]) * view[1][j] + (o[2] - eye[2]) * view[2][j];
                for (int i = 0; i < 3; ++i) tpPos[i] = eye[i] + loc[0] * tpBody[i][0] + loc[1] * tpBody[i][1] + loc[2] * tpBody[i][2];
                tpPairs.fetch_add(1, std::memory_order_relaxed);
            }
        }
        float oc[3] = {o[0], o[1], o[2]};
        if (tpComp) {
            for (int i = 0; i < 3; ++i) { oc[i] = tpPos[i]; for (int j = 0; j < 3; ++j) view[i][j] = tpBody[i][j]; }
            for (int j = 0; j < 3; ++j) view[3][j] = -(oc[0] * view[0][j] + oc[1] * view[1][j] + oc[2] * view[2][j]);
        }
        if (cam_offset_set()) cam_offset(view, oc, unitsPerMetre); // CAMPOS
        return;
    }
    gate(nullptr);
    const bool headOk = headQ && std::isfinite(headQ[0]) && std::isfinite(headQ[1]) && std::isfinite(headQ[2]) && std::isfinite(headQ[3]);
    if (firstEye) { // AIMPOV: the game camera of this pair, before anything of ours
        const float g[3] = {view[0][2], view[1][2], view[2][2]};
        pairGameLook = look_rot(g);
        for (int i = 0; i < 3; ++i) pairGameO[i] = o[i];
    }
    if (firstEye) { // AIMSTATE once per pair; LOOKFWD from the look this pair is drawn with
        pairAimed = false;
        const uintptr_t pc = aimPawnNow ? word(aimPawnNow + kPawnController) : 0;
        const bool want = lookAim.load(std::memory_order_relaxed) && headOk && object(pc) &&
                          (aimMode.load(std::memory_order_relaxed) == 0 || left_trigger_held());
        if (want) pairAimed = state_drive(pc, headQ);
        else state_release(!lookAim.load(std::memory_order_relaxed) ? "head aim off" : "left trigger released");
    }
    if (pairAimed) for (int i = 0; i < 3; ++i) for (int j = 0; j < 3; ++j) view[i][j] = pairBody[i][j];
    if (firstEye) {
        const float f0[3] = {view[0][2], view[1][2], view[2][2]};
        pairLookValid = false;
        if (headOk) {
            float D[3][3];
            head_rot(headQ, D);
            for (int i = 0; i < 3; ++i) { pairLook[i] = 0; for (int k = 0; k < 3; ++k) pairLook[i] += view[i][k] * D[k][2]; }
            pairLookValid = finite3(pairLook);
        }
        // LOOKFWD: the level look direction; looking nearly straight up / down keeps the last one
        const float* f = pairLookValid ? pairLook : f0;
        const float len = sqrtf(f[0] * f[0] + f[1] * f[1]);
        if (len > 0.15f) { pairPush[0] = f[0] / len; pairPush[1] = f[1] / len; pushValid = true; }
        else if (!pushValid) {
            const float l2 = sqrtf(f0[0] * f0[0] + f0[1] * f0[1]);
            if (l2 > 1e-3f) { pairPush[0] = f0[0] / l2; pairPush[1] = f0[1] / l2; pushValid = true; }
        }
    }
    const float cm = unitsPerMetre * 0.01f, fwd = pushValid ? forwardCm.load(std::memory_order_relaxed) * cm : 0.0f;
    const float n[3] = {eye[0] + pairPush[0] * fwd, eye[1] + pairPush[1] * fwd, eye[2] + upCm.load(std::memory_order_relaxed) * cm};
    for (int j = 0; j < 3; ++j) view[3][j] = -(n[0] * view[0][j] + n[1] * view[1][j] + n[2] * view[2][j]);
    if (firstEye) fpPair = true;
    frames.fetch_add(1, std::memory_order_relaxed);
}

bool clamp_eye(float o[3], float unitsPerMetre) {
    if (!fpPair || !eyeValid || !pushValid) return false;
    const float minF = fminf(leanMinCm.load(std::memory_order_relaxed), forwardCm.load(std::memory_order_relaxed)) * unitsPerMetre * 0.01f;
    const float s = (o[0] - eye[0]) * pairPush[0] + (o[1] - eye[1]) * pairPush[1];
    if (s >= minF) return false;
    o[0] += (minF - s) * pairPush[0]; o[1] += (minF - s) * pairPush[1];
    leanClamps.fetch_add(1, std::memory_order_relaxed);
    return true;
}

bool left_trigger_now() { return left_trigger_held(); }

void aim_finder_start() { if (!findUntil.load()) { findUntil.store(GetTickCount64() + 20000); BVR_LOG("[bo] AIMFIND started from the panel (20 s)"); } }
unsigned aim_finder_seconds_left() { const ULONGLONG u = findUntil.load(), n = GetTickCount64(); return u > n ? unsigned((u - n) / 1000) + 1 : 0; }

bool on() { return enabled.load(); }
void set_on(bool v) {
    if (enabled.exchange(v) != v) { BVR_LOG("[bo] first person %s", v ? "ON" : "off"); save(); }
}
void toggle() { set_on(!enabled.load()); } // both sticks clicked twice quickly (overlay thread)

void draw_ui() { // at the top of the panel: it changes the whole view (City)
    bool fp = enabled.load();
    if (ImGui::Checkbox("FIRST PERSON  (or click both sticks twice quickly)", &fp)) set_on(fp);
    if (!fp) {
        bool a3 = lookAim3p.load();
        if (ImGui::Checkbox("   aim with your head while holding the left trigger (third person)", &a3)) { lookAim3p.store(a3); save(); }
        if (overlay::show_tests()) ImGui::TextDisabled("      third-person head aim pairs %u, writes %u", tpPairs.load(), aimWrites.load());
        return;
    }
    bool aim = lookAim.load();
    if (ImGui::Checkbox("   aim with your head  (Batman aims where you look)", &aim)) { lookAim.store(aim); save(); }
    if (aim) {
        bool lt = aimMode.load() == 1;
        if (ImGui::Checkbox("      only while holding the left trigger (gadgets, batclaw)", &lt)) { aimMode.store(lt ? 1 : 0); save(); }
        ImGui::TextDisabled("   gadgets aim where you look; the stick still turns left / right");
    }
    float f = forwardCm.load(), u = upCm.load();
    if (overlay::SliderStep("   camera in front of his face (cm)", &f, -20.0f, 80.0f, "%.0f")) { forwardCm.store(f); save(); }
    if (overlay::SliderStep("   camera up / down (cm)", &u, -40.0f, 40.0f, "%.0f")) { upCm.store(u); save(); }
    float lb = leanMinCm.load();
    if (overlay::SliderStep("   closest to his face when you lean back (cm)", &lb, 0.0f, 40.0f, "%.0f")) { leanMinCm.store(lb); save(); }
    bool hf = followHead.load();
    if (ImGui::Checkbox("   camera follows his head (gliding, animations)", &hf)) { followHead.store(hf); save(); }
    if (hf) {
        float sm = headSmoothS.load();
        if (overlay::SliderStep("   head smoothing (s) - higher = steadier, lags more", &sm, 0.0f, 0.5f, "%.2f")) { headSmoothS.store(sm); save(); }
    }
    if (overlay::show_tests()) {
        ImGui::TextDisabled("      frames %u, head frames %u, steady eye point used %u%s%s", frames.load(), headFrames.load(),
                            headMisses.load(), *gateWhy ? ", paused: " : "", gateWhy);
        ImGui::TextDisabled("      head aim: %s, writes %u, failures %u; read watch %s", stActive ? "steering" : "idle", aimWrites.load(),
                            aimFailures.load(), watchDone.load() ? "done (see log)" : "waiting for the first aim");
        ImGui::TextDisabled("      aimed through the looked-at point %u, parallel (no point) %u", depthAims.load(), plainAims.load());
    }
}

void draw_camera_ui() { // CAMPOS: VIEW section, open the first time (AKVR "camera position")
    ImGui::SetNextItemOpen(true, ImGuiCond_Once);
    if (!ImGui::TreeNode("camera position  (third person)")) return;
    if (enabled.load()) ImGui::TextDisabled("not used in first person");
    float r = camRightCm.load(), u = camUpCm.load(), f = camFwdCm.load();
    if (overlay::SliderStep("left / right (cm)", &r, -300.0f, 300.0f, "%.0f")) { camRightCm.store(r); save(); }
    if (overlay::SliderStep("down / up (cm)", &u, -300.0f, 300.0f, "%.0f")) { camUpCm.store(u); save(); }
    if (overlay::SliderStep("back / forward (cm)", &f, -300.0f, 300.0f, "%.0f")) { camFwdCm.store(f); save(); }
    if (ImGui::Button("Reset position")) { camRightCm.store(0.0f); camUpCm.store(0.0f); camFwdCm.store(0.0f); save(); }
    if (overlay::show_tests()) ImGui::TextDisabled("      moved frames %u", camFrames.load());
    ImGui::TreePop();
}

bool command(const char* a) {
    if (!strcmp(a, "on")) set_on(true);
    else if (!strcmp(a, "off")) set_on(false);
    else if (!strncmp(a, "fwd ", 4)) { forwardCm.store(float(atof(a + 4))); save(); }
    else if (!strncmp(a, "up ", 3)) { upCm.store(float(atof(a + 3))); save(); }
    else if (!strncmp(a, "head ", 5)) { followHead.store(atoi(a + 5) != 0); save(); }
    else if (!strncmp(a, "smooth ", 7)) { headSmoothS.store(float(atof(a + 7))); save(); }
    else if (!strncmp(a, "cam ", 4)) { // CAMPOS: "fp cam <right> <up> <forward>" (cm)
        float r = 0, u = 0, f = 0;
        if (sscanf_s(a + 4, "%f %f %f", &r, &u, &f) != 3) return false;
        camRightCm.store(r); camUpCm.store(u); camFwdCm.store(f); save();
        BVR_LOG("[bo] camera position right %.0f, up %.0f, forward %.0f cm (moved frames %u)", r, u, f, camFrames.load());
        return true;
    }
    else if (strcmp(a, "status")) return false;
    BVR_LOG("[bo] first person %s: forward %.0f cm, up %.0f cm, head %d (smoothing %.2f s) | frames %u, head frames %u, "
            "steady eye point %u, player +0x%X, pawn field +0x%X%s%s", enabled.load() ? "ON" : "off", forwardCm.load(),
            upCm.load(), followHead.load() ? 1 : 0, headSmoothS.load(), frames.load(), headFrames.load(), headMisses.load(),
            playerOff, pawnOff, *gateWhy ? ", paused: " : "", gateWhy);
    return true;
}

} // namespace bvr::bo::first_person
