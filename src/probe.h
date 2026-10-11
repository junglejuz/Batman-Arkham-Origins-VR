#pragma once
#include <windows.h>
#include <cstdint>
#include <string>

namespace bo {
enum class Kind : uint32_t {
    Root, Begin, EndA, EndB, Calc, Ctor, Present, Resize,
    DrawIndexed, Draw, DrawIndexedInstanced, DrawInstanced, DrawAuto,
    DrawIndexedIndirect, DrawIndirect, Dispatch, DispatchIndirect,
    ExecuteList, HullShader, DomainShader, Targets, Viewports, Count
};
constexpr unsigned kKinds = static_cast<unsigned>(Kind::Count);
struct Sample {
    Kind kind{};
    DWORD tid{};
    uint64_t qpc{}, present{};
    uintptr_t self{}, caller{};
    uint32_t argc{}, args[28]{};
    uint32_t valid{};
    float matrices[32]{};
    uint32_t extra[12]{};
};
bool SafeRead(const void* source, void* dest, size_t bytes) noexcept;
void ObserveEngine(Kind kind, uint32_t edx, uintptr_t self, const uint32_t* stack) noexcept;
void Count(Kind kind, uintptr_t self, uintptr_t caller, bool deferred = false) noexcept;
bool Measuring() noexcept;
bool TakeSample(bool camera = false) noexcept;
void Push(const Sample& sample) noexcept;
uint64_t PresentCount() noexcept;
uintptr_t ImageBase() noexcept;
uint32_t ImageSize() noexcept;
void Message(const char* format, ...); // worker thread only
std::string HashFile(const std::wstring& path);
namespace engine {
struct Entry { const char* name; Kind kind; uint32_t rva; const char* prefix; int reloc;
               void* detour; void** original; };
const Entry* Entries(size_t& count);
bool Install();
}
namespace graphics {
bool InstallPresent();
bool InstallContexts();
bool Ready() noexcept;
void Report();
}
}
