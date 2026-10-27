#include "menu_state.h"

#include "core/hooks/pattern_scan.h"
#include "core/util/log.h"

#include <MinHook.h>
#include <windows.h>

#include <atomic>
#include <cstdint>
#include <cstdio>
#include <cstring>

namespace bvr::bo::menu_state {
namespace {
namespace ps = bvr::pattern_scan;
#define PFX "[bo]"

// --- the game's name table (GNames), found live ------------------------------------------------------------
// UE3 keeps every FName's text in a TArray<FNameEntry*> { data, num, max } in a writable section. It is recognised by
// entry 0 = "None" and entry 1 = "ByteProperty" at one text offset inside the entry. Read fresh on every lookup (the
// array grows and may move).
const uint8_t* g_namesArr = nullptr;
int g_textOff = -1;
int g_objNameOff = -1; // UObject: FName index offset; the class pointer is 8 bytes after it (UE3: Name, then Class)

bool text_is(const uint8_t* p, const char* s, bool wide) {
    const size_t n = strlen(s) + 1;
    if (!ps::is_memory_valid(p, wide ? 2 * n : n)) return false;
    for (size_t i = 0; i < n; ++i)
        if ((wide ? reinterpret_cast<const uint16_t*>(p)[i] : p[i]) != uint8_t(s[i])) return false;
    return true;
}

bool g_wideNames = false; // FNameEntry text as UTF-16 (BioShock Infinite: +0x10 wide)
bool names_ok() { return g_namesArr && g_textOff >= 0; }

// Copies the text of name idx into out (always terminated); false when it is not a valid name.
bool name_text(int idx, char* out, size_t n) {
    out[0] = 0;
    if (!names_ok() || idx < 0) return false;
    uint8_t** data = *reinterpret_cast<uint8_t** const*>(g_namesArr);
    const int num = *reinterpret_cast<const int*>(g_namesArr + 4);
    if (idx >= num || !ps::is_memory_valid(data + idx, 4)) return false;
    const uint8_t* e = data[idx];
    if (!e) return false;
    const uint8_t* t = e + g_textOff;
    const size_t cw = g_wideNames ? 2 : 1;
    size_t i = 0;
    for (; i + 1 < n; ++i) {
        if (!ps::is_memory_valid(t + i * cw, cw)) return false;
        const unsigned c = g_wideNames ? reinterpret_cast<const uint16_t*>(t)[i] : t[i];
        if (c == 0) break;
        if (c < 0x20 || c > 0x7E) return false;
        out[i] = static_cast<char>(c);
    }
    out[i] = 0;
    return i > 0 && i + 1 < n;
}

// MS2: page by page (a section can span several memory regions - the first version skipped whole 64 KB blocks that
// crossed one, and found nothing in City), ANSI or UTF-16 text, retried by the worker until found.
bool find_names(const ps::ProcessImage& img, bool logMiss) {
    const auto* dos = reinterpret_cast<const IMAGE_DOS_HEADER*>(img.base);
    const auto* nt = reinterpret_cast<const IMAGE_NT_HEADERS*>(img.base + dos->e_lfanew);
    const IMAGE_SECTION_HEADER* sec = IMAGE_FIRST_SECTION(nt);
    for (unsigned s = 0; s < nt->FileHeader.NumberOfSections; ++s, ++sec) {
        if (!(sec->Characteristics & IMAGE_SCN_MEM_WRITE) || (sec->Characteristics & IMAGE_SCN_MEM_EXECUTE)) continue;
        const uint8_t* lo = img.base + sec->VirtualAddress;
        const size_t len = sec->Misc.VirtualSize & ~size_t(3);
        for (size_t page = 0; page < len; page += 0x1000) {
            if (!ps::is_memory_valid(lo + page, 0x1000 < len - page ? 0x1000 : len - page)) continue;
            const size_t end = page + 0x1000 < len ? page + 0x1000 : len;
            for (size_t o = page; o < end && o + 12 <= len; o += 4) {
                const uint8_t* p = lo + o;
                if (o + 12 > end && !ps::is_memory_valid(p, 12)) break; // the last dwords reach into the next page
                const int num = *reinterpret_cast<const int*>(p + 4), max = *reinterpret_cast<const int*>(p + 8);
                if (num < 2000 || num > 3000000 || max < num || max > 4000000) continue;
                uint8_t** data = *reinterpret_cast<uint8_t** const*>(p);
                if (!ps::is_memory_valid(data, 8)) continue;
                const uint8_t* e0 = data[0];
                const uint8_t* e1 = data[1];
                if (!ps::is_memory_valid(e0, 8) || !ps::is_memory_valid(e1, 8)) continue;
                for (int wide = 0; wide < 2; ++wide)
                    for (int off = 0x4; off <= 0x20; off += 4) {
                        if (text_is(e0 + off, "None", wide) && text_is(e1 + off, "ByteProperty", wide)) {
                            g_wideNames = wide != 0;
                            g_textOff = off;
                            g_namesArr = p;
                            BVR_LOG(PFX " MENUSTATE: name table at %08X (%d names, %s text at entry +0x%X)",
                                    unsigned(uintptr_t(p)), num, wide ? "UTF-16" : "ANSI", off);
                            return true;
                        }
                    }
            }
        }
    }
    if (logMiss) BVR_LOG(PFX " MENUSTATE: name table not found yet - calls are logged with addresses only");
    return false;
}

// The object's name and class name, from the first live object: the offset where Name, Class -> Name and
// Class -> Class -> Name ("Class") all resolve.
void find_object_layout(const uint8_t* obj) {
    if (g_objNameOff >= 0 || !names_ok() || !ps::is_memory_valid(obj, 0x80)) return;
    char t[96];
    for (int off = 0x10; off <= 0x60; off += 4) {
        const uint8_t* cls = *reinterpret_cast<const uint8_t* const*>(obj + off + 8);
        if (!name_text(*reinterpret_cast<const int*>(obj + off), t, sizeof t) || !ps::is_memory_valid(cls, 0x80)) continue;
        const uint8_t* meta = *reinterpret_cast<const uint8_t* const*>(cls + off + 8);
        if (!name_text(*reinterpret_cast<const int*>(cls + off), t, sizeof t) || !ps::is_memory_valid(meta, 0x80)) continue;
        if (name_text(*reinterpret_cast<const int*>(meta + off), t, sizeof t) && strcmp(t, "Class") == 0) {
            g_objNameOff = off;
            BVR_LOG(PFX " MENUSTATE: object name at +0x%X, class at +0x%X", off, off + 8);
            return;
        }
    }
}

void describe(const void* p, char* out, size_t n) {
    const uint8_t* obj = static_cast<const uint8_t*>(p);
    if (!obj) { snprintf(out, n, "none"); return; }
    find_object_layout(obj);
    char name[96] = "?", cls[96] = "?";
    if (g_objNameOff >= 0 && ps::is_memory_valid(obj, g_objNameOff + 12)) {
        name_text(*reinterpret_cast<const int*>(obj + g_objNameOff), name, sizeof name);
        const uint8_t* c = *reinterpret_cast<const uint8_t* const*>(obj + g_objNameOff + 8);
        if (ps::is_memory_valid(c, g_objNameOff + 4)) name_text(*reinterpret_cast<const int*>(c + g_objNameOff), cls, sizeof cls);
        const int number = *reinterpret_cast<const int*>(obj + g_objNameOff + 4);
        if (number > 0) {
            const size_t k = strlen(name);
            snprintf(name + k, sizeof name - k, "_%d", number - 1);
        }
    }
    snprintf(out, n, "%s %s (%08X)", cls, name, unsigned(uintptr_t(obj)));
}

// --- the menu natives ----------------------------------------------------------------------------------------
enum Kind { kCall, kGetObj, kGetBool };
struct Target {
    const char* cls;
    const char* fn;
    Kind kind;
    void* orig = nullptr;
    void* impl = nullptr;
    std::atomic<uint32_t> calls{0};
    uint32_t last = 0xFFFFFFFF; // getters: last answer
    bool skip = false;          // MS4: its function object is not native here
};
// City offline check (batman_city/re/natives.txt; Origins checked live: shared stubs are skipped): each impl below is used by one table entry only
// (OnOutroClose and FrontMost.ShouldFade share theirs with other natives - left out).
Target g_t[] = {
    {"UGFxMoviePlayer", "Start", kCall},
    {"UGFxMoviePlayer", "Close", kCall},
    {"UGFxMoviePlayer", "SetPause", kCall},
    {"UGFxMoviePlayer", "SetFocus", kCall},
    {"UGFxMoviePlayer", "SetMovieCanReceiveInput", kCall},
    {"ARPlayerController", "SetActiveFocusMovie", kCall},
    {"ARPlayerController", "GetActiveFocusMovie", kGetObj},
    {"ARPlayerController", "IsPlayingFullScreenMovie", kGetBool},
    {"ARPlayerController", "IsMovieCapturingInput", kGetBool},
    {"ARPlayerController", "SetRegularGamePauseFlag", kCall},
    {"ARPlayerController", "PauseGamePostLoadIfNeeded", kCall},
    {"URGFxMovieUI", "SaveMenuState", kCall},
    {"URGFxMovieUI", "RestoreMenuState", kCall},
    {"URGFxMovieUI", "ClearMenuState", kCall},
    {"URGFxMovie", "SetGlobalAudioState", kCall},
    {"URGFxMovie", "SetAudioState", kCall},
};
constexpr int kTargets = int(sizeof(g_t) / sizeof(g_t[0]));
SRWLOCK g_lock = SRWLOCK_INIT;
std::atomic<uint32_t> g_lines{0};
std::atomic<int> g_hooked{0};
const void* g_focus = nullptr;
enum { kStart = 0, kClose = 1, kSetFocus = 3, kFullScreen = 7, kRestoreMenu = 12 }; // indices into g_t

// MS2 menu state (LIVE 22:40-22:41, every opening and closing logged): a menu screen = a movie that Starts and takes
// focus in the same moment (pause: + RestoreMenuState + SetRegularGamePauseFlag; map / upgrades: Start + SetFocus) and
// is Closed when it goes. The HUD movie Starts without focus; the loading screen Starts with focus while the
// full-screen movie flag is TRUE (left out).
// MS3 (names live since MS2, City 22:51): pause = RGFxMovieUI_Pause (also calls RestoreMenuState); the map and the
// upgrades = RGFxMovieBackScreen; the main-menu chain = RGFxMovieUI_*; RGFxMovieHudExtendable (the HUD) also Starts with
// focus during the loading screen - never a menu. "Screen menu" = an open menu that is not a pause menu: the map /
// upgrades / database type, which draws its own picture into the shared scene picture (MAPBLIT keys on it).
struct OpenMenu { const void* obj; ULONGLONG at; bool pause; char cls[48]; };
OpenMenu g_open[8] = {};
int g_openN = 0;
const void* g_started = nullptr;
ULONGLONG g_startedMs = 0;
std::atomic<bool> g_menuOpen{false};
std::atomic<ULONGLONG> g_menuOpenAt{0}, g_menuClosedAt{0};
void menu_track(int i, const void* self) { // under g_lock
    const ULONGLONG now = GetTickCount64();
    if (i == kStart) { g_started = self; g_startedMs = now; return; }
    if (i == kSetFocus && self == g_started && now - g_startedMs < 100 && g_t[kFullScreen].last != 1) {
        g_started = nullptr;
        for (int k = 0; k < g_openN; ++k) if (g_open[k].obj == self) return;
        char who[160];
        describe(self, who, sizeof who);
        char cls[48] = {};
        for (int k = 0; k < 47 && who[k] && who[k] != ' '; ++k) cls[k] = who[k];
        if (strstr(cls, "Hud") || strstr(cls, "FrontMost")) {
            BVR_LOG(PFX " MENUSTATE: %s took focus - HUD, not a menu", who);
            return;
        }
        if (g_openN < 8) {
            OpenMenu& m = g_open[g_openN++];
            m = OpenMenu{self, now, strstr(cls, "Pause") != nullptr, {}};
            memcpy(m.cls, cls, sizeof cls);
        }
        g_menuOpen.store(true);
        g_menuOpenAt.store(now);
        BVR_LOG(PFX " MENUSTATE: menu OPEN %s (%d open)", who, g_openN);
        return;
    }
    if (i == kRestoreMenu) {
        for (int k = 0; k < g_openN; ++k) if (g_open[k].obj == self) g_open[k].pause = true;
        return;
    }
    if (i == kClose) {
        for (int k = 0; k < g_openN; ++k) {
            if (g_open[k].obj != self) continue;
            BVR_LOG(PFX " MENUSTATE: menu CLOSED %s %08X after %llu ms (%s)", g_open[k].cls, unsigned(uintptr_t(self)),
                    now - g_open[k].at, g_open[k].pause ? "pause-type menu" : "screen menu: map / upgrades type");
            // MS7 (Origins LIVE 23:33-23:34: the map opens MGFxMovieControllerBackScreen + ...BackScreenTop + ...Map in the
            // same 20 ms and Closes only Map and BackScreen - BackScreenTop stayed "open", MAPBLIT stayed on and JJ was left
            // in flat 2D after the map). Menus opened together close together.
            const ULONGLONG openedAt = g_open[k].at;
            g_open[k] = g_open[--g_openN];
            for (int j = 0; j < g_openN;) {
                const ULONGLONG d = g_open[j].at > openedAt ? g_open[j].at - openedAt : openedAt - g_open[j].at;
                if (d <= 300) {
                    BVR_LOG(PFX " MENUSTATE: menu CLOSED %s %08X with it (opened together)", g_open[j].cls,
                            unsigned(uintptr_t(g_open[j].obj)));
                    g_open[j] = g_open[--g_openN];
                } else {
                    ++j;
                }
            }
            g_menuOpen.store(g_openN > 0);
            g_menuClosedAt.store(now);
            return;
        }
    }
}

std::atomic<unsigned long long> g_movieAt{0}; // ENTRYHOLD (kept apart from the 800-line log limit)
void note(int i, void* self, void* result) {
    if (g_t[i].kind == kGetBool && result && !strcmp(g_t[i].fn, "IsPlayingFullScreenMovie") && ps::is_memory_valid(result, 4) &&
        *static_cast<const uint32_t*>(result))
        g_movieAt.store(GetTickCount64(), std::memory_order_relaxed);
    Target& t = g_t[i];
    const uint32_t n = t.calls.fetch_add(1, std::memory_order_relaxed) + 1;
    if (t.kind == kCall && (i == kStart || i == kClose || i == kSetFocus || i == kRestoreMenu)) {
        AcquireSRWLockExclusive(&g_lock);
        menu_track(i, self);
        ReleaseSRWLockExclusive(&g_lock);
    }
    if (g_lines.load(std::memory_order_relaxed) > 800) return;
    char who[160], what[160] = "";
    AcquireSRWLockExclusive(&g_lock);
    bool log = false;
    if (t.kind == kCall) {
        log = n <= 80;
    } else if (result && ps::is_memory_valid(result, 4)) {
        const uint32_t v = *static_cast<const uint32_t*>(result);
        if (v != t.last) {
            t.last = v;
            log = true;
            if (t.kind == kGetObj) {
                g_focus = reinterpret_cast<const void*>(uintptr_t(v));
                describe(g_focus, what, sizeof what);
            } else {
                snprintf(what, sizeof what, "%s", v ? "TRUE" : "false");
            }
        }
    }
    if (log) describe(self, who, sizeof who);
    ReleaseSRWLockExclusive(&g_lock);
    if (!log) return;
    g_lines.fetch_add(1, std::memory_order_relaxed);
    if (t.kind == kCall)
        BVR_LOG(PFX " MENUSTATE %s.%s on %s (call %u)", t.cls, t.fn, who, n);
    else
        BVR_LOG(PFX " MENUSTATE %s.%s -> %s (on %s, call %u)", t.cls, t.fn, what, who, n);
}

using ExecFn = void(__fastcall*)(void* self, void* edx, void* stack, void* result);
template <int I>
void __fastcall exec_detour(void* self, void* edx, void* stack, void* result) {
    reinterpret_cast<ExecFn>(g_t[I].orig)(self, edx, stack, result);
    note(I, self, result);
}
template <int... I>
struct DetourTable { static inline void* const fns[] = {reinterpret_cast<void*>(&exec_detour<I>)...}; };
using Detours = DetourTable<0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15>;
static_assert(sizeof(Detours::fns) / sizeof(void*) == kTargets, "one detour per target");

// How many native-table entries use this impl (a shared stub would log other natives' calls too).
int table_users(const ps::ProcessImage& img, const void* impl) {
    int users = 0;
    for (const uint8_t* ref : ps::find_references(img, impl)) {
        if (!ps::is_memory_valid(ref - 4, 4)) continue;
        const uint8_t* s = *reinterpret_cast<const uint8_t* const*>(ref - 4);
        char buf[96] = {};
        if (!ps::is_memory_valid(s, 8)) continue;
        for (int k = 0; k < 95 && ps::is_memory_valid(s + k, 1) && s[k]; ++k) buf[k] = static_cast<char>(s[k]);
        if (strstr(buf, "exec")) ++users;
    }
    return users;
}

// --- MS5: the short-name route ---------------------------------------------------------------------------------
// MS4 LIVE 23:22 (Origins): the function-object route took +0xA0 for "the code pointer" because its code range was the
// whole image (.rdata included): 013CC348 is in .rdata, and it wrote over two fields that were not code (one was
// rewritten by the game to 0 a moment later). Fixed below (real .text range). Preferred route first: later UE3 builds
// register natives per class as { "execStart", impl } (no class prefix). A table run (consecutive 8-byte entries whose
// names start with "exec") is accepted for a class only when it holds ALL of that class's anchor names, and only once.
// Then it is hooked like City (code hooks, shared-stub check).
const uint8_t* g_textLo = nullptr;
const uint8_t* g_textHi = nullptr;
void text_range(const ps::ProcessImage& img) {
    if (g_textLo) return;
    const auto* dos = reinterpret_cast<const IMAGE_DOS_HEADER*>(img.base);
    const auto* nt = reinterpret_cast<const IMAGE_NT_HEADERS*>(img.base + dos->e_lfanew);
    const IMAGE_SECTION_HEADER* sec = IMAGE_FIRST_SECTION(nt);
    for (unsigned s = 0; s < nt->FileHeader.NumberOfSections; ++s, ++sec) {
        if (memcmp(sec->Name, ".text", 6) != 0) continue;
        g_textLo = img.base + sec->VirtualAddress;
        g_textHi = g_textLo + sec->Misc.VirtualSize;
        return;
    }
}
bool in_text(uint32_t v) {
    const uint8_t* p = reinterpret_cast<const uint8_t*>(uintptr_t(v));
    return g_textLo && p >= g_textLo && p < g_textHi;
}
// The "exec..." name a table entry points at (empty when it is not one).
bool exec_name(const uint8_t* entry, char* out, size_t n) {
    out[0] = 0;
    if (!ps::is_memory_valid(entry, 8) || !in_text(*reinterpret_cast<const uint32_t*>(entry + 4))) return false;
    const uint8_t* s = *reinterpret_cast<const uint8_t* const*>(entry);
    if (!ps::is_memory_valid(s, 6) || memcmp(s, "exec", 4) != 0) return false;
    size_t i = 0;
    for (; i + 1 < n; ++i) {
        if (!ps::is_memory_valid(s + i, 1)) return false;
        const uint8_t c = s[i];
        if (!c) break;
        if (c < 0x20 || c > 0x7E) return false;
        out[i] = char(c);
    }
    out[i] = 0;
    return i > 4 && i + 1 < n;
}
struct Group { const char* cls; const char* anchors[3]; };
const Group kGroups[] = {
    {"UGFxMoviePlayer", {"Start", "Close", "SetFocus"}},
    {"ARPlayerController", {"SetRegularGamePauseFlag", "GetActiveFocusMovie", nullptr}},
    {"URGFxMovieUI", {"SaveMenuState", "RestoreMenuState", nullptr}},
    {"URGFxMovie", {"SetAudioState", "SetGlobalAudioState", nullptr}},
};
int short_name_route(const ps::ProcessImage& img) {
    text_range(img);
    int queued = 0;
    for (const Group& g : kGroups) {
        char want[96];
        snprintf(want, sizeof want, "exec%s", g.anchors[0]);
        const uint8_t* runLo = nullptr;
        int runs = 0;
        for (const uint8_t* s : ps::find_ascii_string(img, want)) {
            const size_t len = strlen(want);
            if (!ps::is_memory_valid(s - 1, len + 2) || s[-1] != 0 || s[len] != 0) continue; // the whole string only
            for (const uint8_t* ref : ps::find_references(img, s)) {
                char nm[96];
                if (!exec_name(ref, nm, sizeof nm)) continue;
                const uint8_t* lo = ref;
                while (exec_name(lo - 8, nm, sizeof nm)) lo -= 8;
                bool all = true;
                for (const char* a : g.anchors) {
                    if (!a) continue;
                    char aw[96];
                    snprintf(aw, sizeof aw, "exec%s", a);
                    bool in = false;
                    for (const uint8_t* e = lo; exec_name(e, nm, sizeof nm) && !in; e += 8) in = strcmp(nm, aw) == 0;
                    all = all && in;
                }
                if (all && lo != runLo) { runLo = lo; ++runs; }
            }
        }
        if (runs != 1) {
            BVR_LOG(PFX " MENUSTATE: short names: %s table %s", g.cls + 1, runs ? "ambiguous - not used" : "not found");
            continue;
        }
        int inRun = 0;
        char nm[96];
        for (const uint8_t* e = runLo; exec_name(e, nm, sizeof nm); e += 8) ++inRun;
        BVR_LOG(PFX " MENUSTATE: short names: %s table at %08X (%d natives)", g.cls + 1, unsigned(uintptr_t(runLo)), inRun);
        for (int i = 0; i < kTargets; ++i) {
            Target& t = g_t[i];
            if (t.impl || strcmp(t.cls, g.cls) != 0) continue;
            char aw[96];
            snprintf(aw, sizeof aw, "exec%s", t.fn);
            void* fn = nullptr;
            for (const uint8_t* e = runLo; exec_name(e, nm, sizeof nm) && !fn; e += 8)
                if (strcmp(nm, aw) == 0) fn = reinterpret_cast<void*>(uintptr_t(*reinterpret_cast<const uint32_t*>(e + 4)));
            if (!fn) { BVR_LOG(PFX " MENUSTATE: short names: %s.%s not in its table", g.cls + 1, t.fn); continue; }
            const int users = table_users(img, fn);
            if (users > 1) {
                BVR_LOG(PFX " MENUSTATE: %s.%s at %08X is shared by %d natives - not hooked", g.cls + 1, t.fn,
                        unsigned(uintptr_t(fn)), users);
                continue;
            }
            if (MH_CreateHook(fn, Detours::fns[i], &t.orig) != MH_OK || MH_QueueEnableHook(fn) != MH_OK) {
                BVR_LOG(PFX " MENUSTATE: hook %s.%s at %08X FAILED", g.cls + 1, t.fn, unsigned(uintptr_t(fn)));
                continue;
            }
            t.impl = fn;
            ++queued;
        }
    }
    return queued;
}

// --- MS4: the UFunction route --------------------------------------------------------------------------------
// Origins (LIVE 23:17: "0 of 16 menu natives hooked", strings 0 for every name) has no "<Class>exec<Func>" native table
// at all, on disk or live. Every script-callable native still has its UFunction object in the game's object list
// (GObjects): Name = the function, Outer = its class, Func = the code it runs. Found live: GObjects = the TArray whose
// objects resolve Name / Class / Class->Class ("Class"); Outer = the pointer just before Name (checked: "Start" ->
// "GFxMoviePlayer"); Func = the offset where nearly all Functions hold a code pointer and script ones share one value
// (ProcessInternal). The hook swaps that one UFunction's Func (data, no code patch): only that function is affected.
const uint8_t* g_objsArr = nullptr;
int g_outerOff = -1, g_funcOff = -1;
uint32_t g_scriptFunc = 0;

bool name_is(int idx, const char* s) {
    char t[96];
    return name_text(idx, t, sizeof t) && strcmp(t, s) == 0;
}
// The object at o resolves Name / Class / Class->Class == "Class" at name offset off.
bool object_shape(const uint8_t* o, int off) {
    if (!ps::is_memory_valid(o, off + 12)) return false;
    char t[96];
    const uint8_t* cls = *reinterpret_cast<const uint8_t* const*>(o + off + 8);
    if (!name_text(*reinterpret_cast<const int*>(o + off), t, sizeof t) || !ps::is_memory_valid(cls, off + 12)) return false;
    const uint8_t* meta = *reinterpret_cast<const uint8_t* const*>(cls + off + 8);
    if (!name_text(*reinterpret_cast<const int*>(cls + off), t, sizeof t) || !ps::is_memory_valid(meta, off + 4)) return false;
    return name_is(*reinterpret_cast<const int*>(meta + off), "Class");
}
bool find_objects(const ps::ProcessImage& img) {
    if (g_objsArr) return true;
    if (!names_ok()) return false;
    const auto* dos = reinterpret_cast<const IMAGE_DOS_HEADER*>(img.base);
    const auto* nt = reinterpret_cast<const IMAGE_NT_HEADERS*>(img.base + dos->e_lfanew);
    const IMAGE_SECTION_HEADER* sec = IMAGE_FIRST_SECTION(nt);
    for (unsigned s = 0; s < nt->FileHeader.NumberOfSections; ++s, ++sec) {
        if (!(sec->Characteristics & IMAGE_SCN_MEM_WRITE) || (sec->Characteristics & IMAGE_SCN_MEM_EXECUTE)) continue;
        const uint8_t* lo = img.base + sec->VirtualAddress;
        const size_t len = sec->Misc.VirtualSize & ~size_t(3);
        for (size_t page = 0; page < len; page += 0x1000) {
            if (!ps::is_memory_valid(lo + page, 0x1000 < len - page ? 0x1000 : len - page)) continue;
            const size_t end = page + 0x1000 < len ? page + 0x1000 : len;
            for (size_t o = page; o < end && o + 12 <= len; o += 4) {
                const uint8_t* p = lo + o;
                if (o + 12 > end && !ps::is_memory_valid(p, 12)) break;
                if (p == g_namesArr) continue;
                const int num = *reinterpret_cast<const int*>(p + 4), max = *reinterpret_cast<const int*>(p + 8);
                if (num < 10000 || num > 3000000 || max < num || max > 4000000) continue;
                const uint8_t* const* data = *reinterpret_cast<const uint8_t* const* const*>(p);
                if (!ps::is_memory_valid(data, 4 * 64)) continue;
                const uint8_t* first = nullptr;
                for (int k = 0; k < 64 && !first; ++k) first = data[k];
                if (!first || !ps::is_memory_valid(first, 0x70)) continue;
                int off = -1;
                for (int c = 0x10; c <= 0x60 && off < 0; c += 4)
                    if (object_shape(first, c)) off = c;
                if (off < 0) continue;
                int ok = 0, seen = 0;
                for (int k = 0; k < 64; ++k) {
                    if (!data[k]) continue;
                    ++seen;
                    if (object_shape(data[k], off)) ++ok;
                }
                if (seen < 16 || ok * 10 < seen * 9) continue;
                g_objNameOff = off;
                g_objsArr = p;
                BVR_LOG(PFX " MENUSTATE: object list at %08X (%d objects, name at +0x%X)", unsigned(uintptr_t(p)), num, off);
                return true;
            }
        }
    }
    return false;
}
const char* class_object_name(const char* cppName) { return cppName + 1; } // UGFxMoviePlayer -> GFxMoviePlayer

int ufunction_route(const ps::ProcessImage& img, bool logMisses) {
    if (!find_objects(img)) return 0;
    const uint8_t* const* data = *reinterpret_cast<const uint8_t* const* const*>(g_objsArr);
    const int num = *reinterpret_cast<const int*>(g_objsArr + 4);
    text_range(img); // MS5: the real .text range (MS4 used the whole image and took a data field for code)
    const uint8_t* textLo = g_textLo;
    const uint8_t* textHi = g_textHi;
    if (!textLo) return 0;
    const uint8_t* funcClass = nullptr;
    const uint8_t* found[kTargets] = {};
    static const uint8_t* s_sample[1500];
    int samples = 0;
    char t[96];
    for (int i = 0; i < num; ++i) {
        if ((i & 0x3FF) == 0 && !ps::is_memory_valid(data + i, 4 * (i + 0x400 < num ? 0x400 : num - i))) break;
        const uint8_t* o = data[i];
        if (!o || !ps::is_memory_valid(o, g_objNameOff + 12)) continue;
        const uint8_t* cls = *reinterpret_cast<const uint8_t* const*>(o + g_objNameOff + 8);
        if (!funcClass) {
            if (ps::is_memory_valid(cls, g_objNameOff + 4) && name_is(*reinterpret_cast<const int*>(cls + g_objNameOff), "Function"))
                funcClass = cls;
            else
                continue;
        }
        if (cls != funcClass) continue;
        if (samples < 1500) s_sample[samples++] = o;
        if (!name_text(*reinterpret_cast<const int*>(o + g_objNameOff), t, sizeof t)) continue;
        for (int k = 0; k < kTargets; ++k) {
            if (found[k] || strcmp(t, g_t[k].fn) != 0) continue;
            // Outer: the pointer just before Name (UE3), else any of the words before it that names the class.
            const char* want = class_object_name(g_t[k].cls);
            for (int c = g_outerOff >= 0 ? g_outerOff : g_objNameOff - 4; c >= 0x8; c -= 4) {
                const uint8_t* outer = *reinterpret_cast<const uint8_t* const*>(o + c);
                char on[96];
                if (ps::is_memory_valid(outer, g_objNameOff + 4) &&
                    name_text(*reinterpret_cast<const int*>(outer + g_objNameOff), on, sizeof on) && strcmp(on, want) == 0) {
                    if (g_outerOff < 0) {
                        g_outerOff = c;
                        BVR_LOG(PFX " MENUSTATE: function owner at +0x%X (%s.%s)", c, want, t);
                    }
                    found[k] = o;
                    break;
                }
                if (g_outerOff >= 0) break;
            }
        }
    }
    if (found[kStart]) { // MS5 evidence: which words of GFxMoviePlayer.Start point into the code (read only)
        char line[400];
        int n = 0;
        for (int c = 0x10; c <= 0x140 && n < 380; c += 4)
            if (ps::is_memory_valid(found[kStart] + c, 4) && in_text(*reinterpret_cast<const uint32_t*>(found[kStart] + c)))
                n += snprintf(line + n, sizeof line - n, " +%X=%08X", c, *reinterpret_cast<const uint32_t*>(found[kStart] + c));
        line[n] = 0;
        static bool s_said = false;
        if (!s_said) { s_said = true; BVR_LOG(PFX " MENUSTATE: GFxMoviePlayer.Start object %08X code words:%s", unsigned(uintptr_t(found[kStart])), n ? line : " none"); }
    }
    // Func (MS6, Origins LIVE 23:28: GFxMoviePlayer.Start holds code at +0x88 = 0xBB2260, disassembled: the execStart
    // thunk - reads the optional bool through GNatives 0x1812C10 and calls on; +0x128 is the next object's +0x88). The
    // statistical rule (MS4/MS5) failed both ways. Now: the one offset inside the object where Start, Close and SetFocus
    // ALL hold real, pairwise different code addresses (.text only). Anything else = no hook.
    if (g_funcOff < 0 && found[kStart] && found[kClose] && found[kSetFocus]) {
        int hits = 0, at = -1;
        for (int c = 0x30; c <= 0x100; c += 4) {
            uint32_t v[3];
            bool ok = true;
            const uint8_t* objs[3] = {found[kStart], found[kClose], found[kSetFocus]};
            for (int k = 0; k < 3 && ok; ++k) {
                ok = ps::is_memory_valid(objs[k] + c, 4);
                if (ok) { v[k] = *reinterpret_cast<const uint32_t*>(objs[k] + c); ok = in_text(v[k]); }
            }
            if (ok && v[0] != v[1] && v[1] != v[2] && v[0] != v[2]) { ++hits; at = c; }
        }
        if (hits == 1) {
            g_funcOff = at;
            BVR_LOG(PFX " MENUSTATE: function code pointer at +0x%X (Start / Close / SetFocus: %08X %08X %08X)", at,
                    *reinterpret_cast<const uint32_t*>(found[kStart] + at), *reinterpret_cast<const uint32_t*>(found[kClose] + at),
                    *reinterpret_cast<const uint32_t*>(found[kSetFocus] + at));
        } else {
            static int s_said = 0;
            if (s_said++ < 3) BVR_LOG(PFX " MENUSTATE: function code pointer offset NOT found (%d candidate offsets) - no hooks", hits);
        }
    }
    if (g_funcOff < 0) return 0;
    int hooked = 0;
    for (int k = 0; k < kTargets; ++k) {
        Target& tg = g_t[k];
        if (tg.impl || tg.skip) continue;
        if (!found[k]) {
            if (logMisses) BVR_LOG(PFX " MENUSTATE: function %s.%s not in the object list", class_object_name(tg.cls), tg.fn);
            continue;
        }
        uint32_t* slot = reinterpret_cast<uint32_t*>(const_cast<uint8_t*>(found[k]) + g_funcOff);
        const uint32_t fn = *slot;
        if (fn == g_scriptFunc || !in_text(fn)) {
            BVR_LOG(PFX " MENUSTATE: %s.%s is not native here (code %08X) - not hooked", class_object_name(tg.cls), tg.fn, fn);
            tg.skip = true; // do not retry
            continue;
        }
        tg.orig = reinterpret_cast<void*>(uintptr_t(fn));
        InterlockedExchange(reinterpret_cast<volatile LONG*>(slot), LONG(uintptr_t(Detours::fns[k])));
        tg.impl = reinterpret_cast<void*>(uintptr_t(fn));
        ++hooked;
        BVR_LOG(PFX " MENUSTATE:   %s.%s hooked through its function object %08X (code %08X)", class_object_name(tg.cls),
                tg.fn, unsigned(uintptr_t(found[k])), fn);
    }
    return hooked;
}

DWORD WINAPI install_worker(void*) {
    ps::ProcessImage img;
    if (!ps::capture_main_module(img)) { BVR_LOG(PFX " MENUSTATE: no module image"); return 0; }
    const bool namesNow = find_names(img, false);
    int queued = 0;
    for (int i = 0; i < kTargets; ++i) {
        Target& t = g_t[i];
        ps::NativeScanResult r;
        if (!ps::find_native_function_ex(img, ps::kNativeTableUE3, t.cls, t.fn, r, true) || !r.function) {
            BVR_LOG(PFX " MENUSTATE: %sexec%s not found (strings %zu, refs %zu)", t.cls, t.fn, r.stringMatches, r.tableRefs);
            continue;
        }
        const int users = table_users(img, r.function);
        if (users > 1) {
            BVR_LOG(PFX " MENUSTATE: %sexec%s at %08X is shared by %d natives - not hooked", t.cls, t.fn,
                    unsigned(uintptr_t(r.function)), users);
            continue;
        }
        if (MH_CreateHook(r.function, Detours::fns[i], &t.orig) != MH_OK || MH_QueueEnableHook(r.function) != MH_OK) {
            BVR_LOG(PFX " MENUSTATE: hook %sexec%s at %08X FAILED", t.cls, t.fn, unsigned(uintptr_t(r.function)));
            continue;
        }
        t.impl = r.function;
        ++queued;
    }
    const MH_STATUS a = queued ? MH_ApplyQueued() : MH_OK;
    if (a == MH_OK) g_hooked.store(queued);
    BVR_LOG(PFX " MENUSTATE: %d of %d menu natives hooked (%s)", queued, kTargets, MH_StatusToString(a));
    for (int i = 0; i < kTargets; ++i)
        if (g_t[i].impl) BVR_LOG(PFX " MENUSTATE:   %sexec%s @%08X", g_t[i].cls, g_t[i].fn, unsigned(uintptr_t(g_t[i].impl)));
    // MS2: the name table may not exist yet this early (run 22:39: searched 3 s after start). MS4: no native name table
    // (Origins) -> the function objects, retried with it every 3 s for 2 minutes until Start / Close / SetFocus are in.
    if (!namesNow) BVR_LOG(PFX " MENUSTATE: name table not found yet - retrying every 3 s");
    if (queued == 0) {
        BVR_LOG(PFX " MENUSTATE: no \"<Class>exec<Func>\" table - trying the short \"exec<Func>\" tables");
        queued = short_name_route(img);
        const MH_STATUS b = queued ? MH_ApplyQueued() : MH_OK;
        if (b != MH_OK) { BVR_LOG(PFX " MENUSTATE: short-name hooks FAILED to apply (%s)", MH_StatusToString(b)); queued = 0; }
        if (queued) {
            g_hooked.store(queued);
            BVR_LOG(PFX " MENUSTATE: %d menu natives hooked through the short-name tables", queued);
        } else {
            BVR_LOG(PFX " MENUSTATE: no short-name tables - using the game's function objects instead");
        }
    }
    int total = queued;
    for (int k = 0; k < 40; ++k) {
        const bool coreIn = g_t[kStart].impl && g_t[kClose].impl && g_t[kSetFocus].impl;
        if (names_ok() && (queued > 0 || coreIn)) break;
        if (k) Sleep(3000);
        if (!names_ok() && !find_names(img, k == 39)) continue;
        if (queued == 0) {
            const int h = ufunction_route(img, k == 39 || (g_t[kStart].impl && g_t[kClose].impl && g_t[kSetFocus].impl));
            if (h > 0) {
                total += h;
                g_hooked.store(total);
                BVR_LOG(PFX " MENUSTATE: %d menu functions hooked through their function objects (%d in all)", h, total);
            }
        }
    }
    if (total == 0) BVR_LOG(PFX " MENUSTATE: NO menu functions hooked - the menu signal is OFF (map fix cannot switch on)");
    return 0;
}

} // namespace

// The name scans read the whole game image (~50 MB, several passes): own thread, never the game's.
void install() {
    if (HANDLE t = CreateThread(nullptr, 0, install_worker, nullptr, 0, nullptr)) CloseHandle(t);
    else BVR_LOG(PFX " MENUSTATE: no worker thread - not installed");
}

void status_line(char* out, unsigned n) {
    uint32_t calls = 0;
    for (const Target& t : g_t) calls += t.calls.load(std::memory_order_relaxed);
    char focus[160];
    AcquireSRWLockShared(&g_lock);
    describe(g_focus, focus, sizeof focus);
    ReleaseSRWLockShared(&g_lock);
    snprintf(out, n, "MENUSTATE: %d natives hooked, %u calls, name table %s, focus movie %s", g_hooked.load(), calls,
             names_ok() ? "found" : "missing", focus);
}

bool live() {
    return g_hooked.load(std::memory_order_relaxed) > 0 && g_t[kStart].impl && g_t[kClose].impl && g_t[kSetFocus].impl;
}
bool menu_open() { return g_menuOpen.load(std::memory_order_relaxed); }
bool screen_menu_open() {
    if (!g_menuOpen.load(std::memory_order_relaxed)) return false;
    AcquireSRWLockShared(&g_lock);
    bool any = false;
    // MS8 (Origins 23:38: the title chain RGFxMovieUI_PressStart / Login / StorageSelect ... counted as screen menus and
    // flipped the view 2D / 3D while booting): only the map-type classes count - City RGFxMovieBackScreen, Origins
    // MGFxMovieControllerBackScreen / ...Map.
    for (int k = 0; k < g_openN; ++k)
        any = any || (!g_open[k].pause && (strstr(g_open[k].cls, "BackScreen") || strstr(g_open[k].cls, "Map")));
    ReleaseSRWLockShared(&g_lock);
    return any;
}
unsigned long long menu_open_at() { return g_menuOpenAt.load(std::memory_order_relaxed); }
unsigned long long menu_closed_at() { return g_menuClosedAt.load(std::memory_order_relaxed); }
void clear_stale(const char* why) {
    AcquireSRWLockExclusive(&g_lock);
    const int n = g_openN;
    g_openN = 0;
    g_menuOpen.store(false);
    ReleaseSRWLockExclusive(&g_lock);
    if (n) BVR_LOG(PFX " MENUSTATE: %d open menu(s) dropped - %s", n, why);
}

void dump() {
    char line[256];
    status_line(line, sizeof line);
    BVR_LOG(PFX " %s", line);
    for (const Target& t : g_t)
        BVR_LOG(PFX " MENUSTATE   %s.%s: %s, %u calls", t.cls, t.fn, t.impl ? "hooked" : "not hooked",
                t.calls.load(std::memory_order_relaxed));
}

unsigned long long movie_seen_at() { return g_movieAt.load(std::memory_order_relaxed); }

} // namespace bvr::bo::menu_state
