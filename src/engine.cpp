#include "probe.h"
#include <MinHook.h>
#include <array>
#include <cstring>

namespace bo::engine {
void* root_original;
void* begin_original;
void* enda_original;
void* endb_original;
void* calc_original;
void* ctor_original;

extern "C" void __cdecl BO_ObserveEngine(uint32_t kind, uint32_t edx,
                                         uintptr_t self, const uint32_t* stack) noexcept {
    ObserveEngine(static_cast<Kind>(kind), edx, self, stack);
}

// Opaque entry probe: original registers, flags, x87/SSE state, arguments,
// return address and stack cleanup all reach the original unchanged. No
// inferred decompiler prototype is used to call the retail function.
#define BO_ENTRY(name, index, original) \
__declspec(naked) void name() { __asm { \
    pushfd \
    pushad \
    mov ebx, esp \
    sub esp, 528 \
    and esp, 0xfffffff0 \
    fxsave [esp] \
    lea eax, [ebx + 36] \
    push eax \
    push dword ptr [ebx + 24] \
    push dword ptr [ebx + 20] \
    push index \
    call BO_ObserveEngine \
    add esp, 16 \
    fxrstor [esp] \
    mov esp, ebx \
    popad \
    popfd \
    jmp dword ptr [original] \
} }
BO_ENTRY(RootEntry, 0, root_original)
BO_ENTRY(BeginEntry, 1, begin_original)
BO_ENTRY(EndAEntry, 2, enda_original)
BO_ENTRY(EndBEntry, 3, endb_original)
BO_ENTRY(CalcEntry, 4, calc_original)
BO_ENTRY(CtorEntry, 5, ctor_original)
#undef BO_ENTRY

const Entry* Entries(size_t& count) {
    static const Entry entries[] = {
        {"viewport_root", Kind::Root, 0x27ed90, "6aff687221270164a1000000005081ec", 3, RootEntry, &root_original},
        {"begin_execute", Kind::Begin, 0x265560, "8b49048b018b5060ffd2b808000000c3", -1, BeginEntry, &begin_original},
        {"end_execute_a", Kind::EndA, 0x265590, "8b49048b018b50646a006a00ffd2b808", -1, EndAEntry, &enda_original},
        {"end_execute_b", Kind::EndB, 0x265710, "8b41088b49045051e8f3feffff83c408", -1, EndBEntry, &endb_original},
        {"calc_scene_view", Kind::Calc, 0x3d40f0, "558bec83e4f06aff6891b7270164a100", 9, CalcEntry, &calc_original},
        {"scene_view_ctor", Kind::Ctor, 0x5cfdc0, "558bec83e4f06aff68f2a2280164a100", 9, CtorEntry, &ctor_original}
    };
    count = std::size(entries);
    return entries;
}

bool Install() {
    static bool installed = false;
    if (installed) return true;
    size_t count = 0;
    const auto* entries = Entries(count);
    std::array<void*, 6> targets{};
    for (size_t i = 0; i < count; ++i) {
        const auto& entry = entries[i];
        unsigned char expected[16]{}, actual[16]{};
        for (unsigned n = 0; n < 16; ++n) {
            char pair[] = {entry.prefix[n*2], entry.prefix[n*2+1], 0};
            expected[n] = static_cast<unsigned char>(strtoul(pair, nullptr, 16));
        }
        if (entry.reloc >= 0) {
            uint32_t value;
            memcpy(&value, expected + entry.reloc, sizeof(value));
            value += static_cast<uint32_t>(ImageBase() - 0x400000u);
            memcpy(expected + entry.reloc, &value, sizeof(value));
        }
        targets[i] = reinterpret_cast<void*>(ImageBase() + entry.rva);
        if (!SafeRead(targets[i], actual, sizeof(actual)) || memcmp(expected, actual, 16)) {
            char bytes[33]{};
            for (unsigned n = 0; n < 16; ++n) sprintf_s(bytes+n*2, 3, "%02x", actual[n]);
            Message("engine guard refused %s base=%08x rva=%08x actual=%s", entry.name,
                    static_cast<unsigned>(ImageBase()), entry.rva, bytes);
            return false;
        }
    }
    size_t created = 0;
    for (; created < count; ++created) {
        auto result = MH_CreateHook(targets[created], entries[created].detour, entries[created].original);
        if (result != MH_OK) {
            Message("engine create refused %s: %s", entries[created].name, MH_StatusToString(result));
            break;
        }
    }
    bool ok = created == count;
    if (ok) for (auto* target : targets) ok = MH_QueueEnableHook(target) == MH_OK && ok;
    if (ok) ok = MH_ApplyQueued() == MH_OK;
    if (!ok) {
        for (size_t i = 0; i < created; ++i) MH_QueueDisableHook(targets[i]);
        MH_ApplyQueued();
        for (size_t i = 0; i < created; ++i) MH_RemoveHook(targets[i]);
        return false;
    }
    installed = true;
    for (size_t i = 0; i < count; ++i)
        Message("engine hook installed %s target=%08x trampoline=%08x; hits not yet proved",
                entries[i].name, reinterpret_cast<unsigned>(targets[i]),
                reinterpret_cast<unsigned>(*entries[i].original));
    return true;
}
}
