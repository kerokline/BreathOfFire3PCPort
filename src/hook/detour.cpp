#include "hook/detour.h"

#include <windows.h>

#include <cstring>

#include "bof3/symbols.gen.h"
#include "hook/log.h"

namespace bof3 {
namespace {

constexpr unsigned kJmpLen = 5;

int g_enabled = 0;
int g_disabled = 0;

constexpr int kMaxOwned = 4096;
std::uint32_t g_owned[kMaxOwned];

bool NameListed(const char* list, const char* name) {
    size_t len = std::strlen(name);
    for (const char* p = list; *p;) {
        while (*p == ',' || *p == ' ') ++p;
        const char* end = p;
        while (*end && *end != ',' && *end != ' ') ++end;
        size_t n = static_cast<size_t>(end - p);
        if ((n == 1 && *p == '*') || (n == len && std::memcmp(p, name, len) == 0)) return true;
        p = end;
    }
    return false;
}

bool WantsOriginal(const char* name) {
    char list[2048];
    DWORD n = GetEnvironmentVariableA("BOF3X_ORIGINAL", list, sizeof list);
    if (n == 0) return false;
    if (n >= sizeof list) Fatal("BOF3X_ORIGINAL is longer than %u bytes", (unsigned)sizeof list);
    return NameListed(list, name);
}

void WriteJmp(const char* name, std::uint8_t* at, const std::uint8_t* to) {
    DWORD old = 0;
    if (!VirtualProtect(at, kJmpLen, PAGE_EXECUTE_READWRITE, &old))
        Fatal("%s: VirtualProtect(%p) failed, error %lu", name, (void*)at, GetLastError());
    std::int32_t rel = static_cast<std::int32_t>(to - (at + kJmpLen));
    at[0] = 0xE9;
    std::memcpy(at + 1, &rel, sizeof rel);
    DWORD ignored = 0;
    VirtualProtect(at, kJmpLen, old, &ignored);
    FlushInstructionCache(GetCurrentProcess(), at, kJmpLen);
}

}  // namespace

void VerifyImage() {
    auto* base = reinterpret_cast<const std::uint8_t*>(GetModuleHandleW(nullptr));
    if (reinterpret_cast<std::uintptr_t>(base) != image::kBase)
        Fatal("main module is at %p, not 0x%X: this is not BOF3.exe, or it was rebased",
              (void*)base, (unsigned)image::kBase);
    auto* dos = reinterpret_cast<const IMAGE_DOS_HEADER*>(base);
    auto* nt = reinterpret_cast<const IMAGE_NT_HEADERS32*>(base + dos->e_lfanew);
    if (nt->Signature != IMAGE_NT_SIGNATURE ||
        nt->FileHeader.TimeDateStamp != image::kTimestamp ||
        nt->OptionalHeader.SizeOfImage != image::kSizeOfImage)
        Fatal("main module is not the BOF3.exe that symbols.toml describes "
              "(timestamp 0x%lX size 0x%lX, expected 0x%X / 0x%X). Every address "
              "would be wrong; refusing to patch.",
              nt->FileHeader.TimeDateStamp, nt->OptionalHeader.SizeOfImage,
              (unsigned)image::kTimestamp, (unsigned)image::kSizeOfImage);
    Log("image verified: base 0x%X timestamp 0x%X", (unsigned)image::kBase,
        (unsigned)image::kTimestamp);
}

void Inject(const char* name, std::uint32_t original, void* ours) {
    auto* orig = reinterpret_cast<std::uint8_t*>(static_cast<std::uintptr_t>(original));
    auto* mine = static_cast<std::uint8_t*>(ours);
    if (g_enabled + g_disabled == kMaxOwned) Fatal("%s: more than %d injected functions", name, kMaxOwned);
    g_owned[g_enabled + g_disabled] = original;
    if (WantsOriginal(name)) {
        WriteJmp(name, mine, orig);
        ++g_disabled;
        Log("inject  OFF  %-24s ours %p -> original 0x%08X", name, ours, (unsigned)original);
    } else {
        WriteJmp(name, orig, mine);
        ++g_enabled;
        Log("inject  ON   %-24s original 0x%08X -> ours %p", name, (unsigned)original, ours);
    }
}

void* CloneOriginal(const char* name, std::uint32_t original, std::uint32_t size,
                    const CloneCall* calls, int n_calls) {
    auto* orig = reinterpret_cast<const std::uint8_t*>(static_cast<std::uintptr_t>(original));
    if (IsOwned(original)) Fatal("%s: CloneOriginal after Inject - the entry is already a jmp", name);
    // A tracer or debugger patch at the entry would be copied as a relative
    // jmp or a breakpoint, and run wrong from the new address.
    if (orig[0] == 0xE9 || orig[0] == 0xE8 || orig[0] == 0xCC)
        Fatal("%s: entry 0x%08X is already patched (%02X), cannot clone", name, (unsigned)original, orig[0]);
    void* copy = VirtualAlloc(nullptr, size, MEM_COMMIT | MEM_RESERVE, PAGE_EXECUTE_READWRITE);
    if (!copy) Fatal("%s: VirtualAlloc(%u) failed, error %lu", name, (unsigned)size, GetLastError());
    std::memcpy(copy, orig, size);
    for (int i = 0; i < n_calls; ++i) {
        const std::uint32_t at = calls[i].offset;
        // E8 call or E9 tail jump: the same rel32, re-aimed the same way.
        if (at + kJmpLen > size || (orig[at] != 0xE8 && orig[at] != 0xE9))
            Fatal("%s: no relative call or jmp at +0x%X to re-aim", name, (unsigned)at);
        std::int32_t rel;
        std::memcpy(&rel, orig + at + 1, sizeof rel);
        const std::uint8_t* target = calls[i].target
            ? static_cast<const std::uint8_t*>(calls[i].target) : orig + at + kJmpLen + rel;
        rel = static_cast<std::int32_t>(target - (static_cast<std::uint8_t*>(copy) + at + kJmpLen));
        std::memcpy(static_cast<std::uint8_t*>(copy) + at + 1, &rel, sizeof rel);
    }
    FlushInstructionCache(GetCurrentProcess(), copy, size);
    Log("shadow      %-24s original 0x%08X cloned to %p, %u bytes", name, (unsigned)original, copy, (unsigned)size);
    return copy;
}

bool WantsShadow(const char* name) {
    char list[2048];
    DWORD n = GetEnvironmentVariableA("BOF3X_SHADOW", list, sizeof list);
    if (n == 0) return false;
    if (n >= sizeof list) Fatal("BOF3X_SHADOW is longer than %u bytes", (unsigned)sizeof list);
    return NameListed(list, name);
}

bool IsOwned(std::uint32_t original) {
    for (int i = 0; i < g_enabled + g_disabled; ++i)
        if (g_owned[i] == original) return true;
    return false;
}

void InjectReport() {
    Log("inject: %d ours, %d left original by BOF3X_ORIGINAL", g_enabled, g_disabled);
}

}  // namespace bof3
