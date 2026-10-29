#include "hook/detour.h"

#include <windows.h>

#include <cstring>

#include "bof3/symbols.gen.h"
#include "hook/bind.h"
#include "hook/code_bytes.h"
#include "hook/code_names.h"
#include "hook/exe_layout.h"
#include "hook/host.h"
#include "hook/log.h"

namespace bof3 {
namespace {

// E8/E9 rel32: one opcode byte and a displacement counted from the end of the
// five-byte instruction. Also the length of a CALL rel32.
constexpr unsigned kJmpLen = 5;

int g_enabled = 0;
int g_disabled = 0;

// Every address passed to Inject, both directions, for IsOwned.
constexpr int kMaxOwned = 16384;  // 4,096 hit at 4,097 ours (round 10 SC5 merge, 2026-09-27); ~10,200 real functions in the exe
std::uint32_t g_owned[kMaxOwned];
bool g_owned_on[kMaxOwned];  // true where the detour is ON (not left original)

// A name, `*` for every name, or `-NAME` to take one back out: `*,-LoadDatFile`
// is every name but LoadDatFile. An exclusion wins wherever it stands.
bool NameListed(const char* list, const char* name) {
    size_t len = std::strlen(name);
    bool listed = false;
    for (const char* p = list; *p;) {
        while (*p == ',' || *p == ' ') ++p;
        const char* end = p;
        while (*end && *end != ',' && *end != ' ') ++end;
        size_t n = static_cast<size_t>(end - p);
        if (n == len + 1 && *p == '-' && std::memcmp(p + 1, name, len) == 0) return false;
        if ((n == 1 && *p == '*') || (n == len && std::memcmp(p, name, len) == 0)) listed = true;
        p = end;
    }
    return listed;
}

bool WantsOriginal(const char* name) {
    char list[8192];  // a whole round of takeovers fits: 119 names is 2,061 characters
    DWORD n = GetEnvironmentVariableA("BOF3X_ORIGINAL", list, sizeof list);
    if (n == 0) return false;
    if (n >= sizeof list) Fatal("BOF3X_ORIGINAL is longer than %u bytes", (unsigned)sizeof list);
    return NameListed(list, name);
}

// BOF3X_OFF, the player's switch (docs/launcher-settings.md, "off="): the same
// list syntax as BOF3X_ORIGINAL, read by both hosts, for code names only - the
// first argument of a PatchBytes or a RetargetCall, whose original behaviour
// is in our code already. Read once: injection runs on one thread, and the
// environment does not change under it. Empty when unset.
const char* OffList() {
    static char list[8192];
    static bool read = false;
    if (!read) {
        DWORD n = GetEnvironmentVariableA("BOF3X_OFF", list, sizeof list);
        if (n >= sizeof list) Fatal("BOF3X_OFF is longer than %u bytes", (unsigned)sizeof list - 1);
        if (n == 0) list[0] = '\0';
        read = true;
    }
    return list;
}

bool WantsOff(const char* name) { return NameListed(OffList(), name); }

// True if `name` itself stands in the list, bare or as `-name` - not only
// through `*`. For the names BOF3X_OFF must not take (a detour's).
bool NameNamed(const char* list, const char* name) {
    size_t len = std::strlen(name);
    for (const char* p = list; *p;) {
        while (*p == ',' || *p == ' ') ++p;
        const char* end = p;
        while (*end && *end != ',' && *end != ' ') ++end;
        size_t n = static_cast<size_t>(end - p);
        if (n > 0 && *p == '-') ++p, --n;
        if (n == len && std::memcmp(p, name, len) == 0) return true;
        p = end;
    }
    return false;
}

// True if the n characters at `s` are one of the code names src/ has - the
// generated list (code_names.h, tools/ledger_switches.py), sorted by strcmp.
bool KnownCodeName(const char* s, size_t n) {
    int lo = 0, hi = code_names::kCount;
    while (lo < hi) {
        const int mid = (lo + hi) / 2;
        const char* k = code_names::kNames[mid];
        int c = std::strncmp(k, s, n);
        if (c == 0) c = k[n] == '\0' ? 0 : 1;
        if (c == 0) return true;
        if (c < 0) lo = mid + 1; else hi = mid;
    }
    return false;
}

// Every code name a PatchBytes or a (not instrument) RetargetCall asked for,
// once each, with the variable that turned it off (null: on). InjectReport
// checks BOF3X_OFF's names against this. 83 names in src/ on 2026-10-28.
struct CodeName {
    const char* name;
    const char* off;
};
constexpr int kMaxCodeNames = 512;
CodeName g_code[kMaxCodeNames];
int g_code_count = 0;

// The variable that turns the code name `name` off - "BOF3X_ORIGINAL" (the
// hosted A/B switch, unchanged), "BOF3X_OFF" (the player's), or null for on.
// A name listed in both is BOF3X_ORIGINAL's, as before BOF3X_OFF existed.
const char* CodeOff(const char* name) {
    const char* off = WantsOriginal(name) ? "BOF3X_ORIGINAL" : WantsOff(name) ? "BOF3X_OFF" : nullptr;
    for (int i = 0; i < g_code_count; ++i)
        if (std::strcmp(g_code[i].name, name) == 0) return off;
    if (g_code_count == kMaxCodeNames) Fatal("%s: more than %d code names", name, kMaxCodeNames);
    // The list BOF3X_OFF is checked against must hold every name the code
    // asks for; a name it lacks is a stale src/hook/code_names.h.
    if (!KnownCodeName(name, std::strlen(name)))
        Fatal("%s: a PatchBytes or RetargetCall name that src/hook/code_names.h does not list; "
              "run python tools/ledger_switches.py and rebuild",
              name);
    g_code[g_code_count++] = {name, off};
    return off;
}

// `jmp to` (E9 rel32) over the first five bytes at `at`, protection restored.
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

void Inject(const char* name, std::uint32_t original, void* ours, bool instrument) {
    // `original` is a VA, a name. Hosted the jmp goes at it, in BOF3.exe's
    // code. In the exe host there is no code to write: .stub is the no-access
    // trap, and every place of the VA - a code word of .data, a Code() of
    // ours - is already its place in the bind table, a jmp to the function that must
    // be this one (bind.cpp; docs/relocatable-image-design.md 4.1). The pair
    // is recorded, as hosted, for IsOwned and IsEnabled.
    auto* orig = reinterpret_cast<std::uint8_t*>(static_cast<std::uintptr_t>(Img(original)));
    auto* mine = static_cast<std::uint8_t*>(ours);
    if (!InCode(original)) Fatal("%s: Inject at 0x%08X, outside Capcom's code range", name, (unsigned)original);
    if (g_enabled + g_disabled == kMaxOwned) Fatal("%s: more than %d injected functions", name, kMaxOwned);
    // BOF3X_OFF takes code names only: a detour's original behaviour is
    // Capcom's function, which the player's build does not have. `*` does not
    // reach a detour; a detour named in the list stops the game here.
    if (!instrument && NameNamed(OffList(), name))
        Fatal("BOF3X_OFF names %s, which is a detour. BOF3X_OFF turns off code names only. "
              "To run Capcom's original function, use BOF3X_ORIGINAL=%s in the hosted build "
              "(bof3x-launcher --host bof3).",
              name, name);
    g_owned[g_enabled + g_disabled] = original;
    g_owned_on[g_enabled + g_disabled] = instrument || !WantsOriginal(name);
    if (IsExeHost()) {
        // BOF3X_ORIGINAL is refused in this host (exe_host.cpp), so every
        // Inject is on; an instrument wraps Capcom's code, which is not here.
        if (instrument) Fatal("%s: an instrument Inject at 0x%08X in bof3x.exe, which maps no code of Capcom's", name, (unsigned)original);
        Bind_Check(name, original, ours);
        ++g_enabled;
        Log("inject  ON   %-24s original 0x%08X = ours %p (bound)", name, (unsigned)original, ours);
        return;
    }
    if (!g_owned_on[g_enabled + g_disabled]) {
        WriteJmp(name, mine, orig);
        ++g_disabled;
        Log("inject  OFF  %-24s ours %p -> original 0x%08X", name, ours, (unsigned)original);
    } else {
        Operands_BeforeJmp(original);   // hosted: values of ours under these five bytes, compared while the code holds them
        WriteJmp(name, orig, mine);
        ++g_enabled;
        Log("inject  ON   %-24s original 0x%08X -> ours %p", name, (unsigned)original, ours);
    }
}

bool RetargetCall(const char* name, std::uint32_t site, std::uint32_t expected, void* ours, bool instrument) {
    // The exe host has no Capcom code at `site`: nothing is written. The
    // divergence that re-aims the call keeps which draw it chose as a value of
    // its own from this answer, and its screens read that
    // (docs/operand-store.md); hosted, Operands_Check compares it with the call.
    const bool exe = IsExeHost();
    if (exe && !InCode(site)) Fatal("%s: RetargetCall at 0x%08X, outside Capcom's code", name, (unsigned)site);
    auto* at = reinterpret_cast<std::uint8_t*>(static_cast<std::uintptr_t>(Img(site)));   // hosted only: the identity
    std::int32_t rel = 0;
    if (!exe) {
        std::memcpy(&rel, at + 1, sizeof rel);
        // E8 rel32 is CALL; only the displacement is rewritten below.
        if (at[0] != 0xE8 || site + kJmpLen + static_cast<std::uint32_t>(rel) != expected)
            Fatal("%s: 0x%08X is not a call to 0x%08X", name, (unsigned)site, (unsigned)expected);
    }
    if (const char* off = instrument ? nullptr : CodeOff(name)) {
        Log("retarget OFF %-24s call at 0x%08X left on 0x%08X (%s)", name, (unsigned)site, (unsigned)expected, off);
        return false;
    }
    if (exe) {
        Log("retarget ON  %-24s call at 0x%08X: 0x%08X -> ours %p (a value of ours; no code in this host)", name,
            (unsigned)site, (unsigned)expected, ours);
        return true;
    }
    DWORD old = 0;
    if (!VirtualProtect(at, kJmpLen, PAGE_EXECUTE_READWRITE, &old))
        Fatal("%s: VirtualProtect(%p) failed, error %lu", name, (void*)at, GetLastError());
    rel = static_cast<std::int32_t>(static_cast<std::uint8_t*>(ours) - (at + kJmpLen));
    std::memcpy(at + 1, &rel, sizeof rel);
    DWORD ignored = 0;
    VirtualProtect(at, kJmpLen, old, &ignored);
    FlushInstructionCache(GetCurrentProcess(), at, kJmpLen);
    Log("retarget ON  %-24s call at 0x%08X: 0x%08X -> ours %p", name, (unsigned)site, (unsigned)expected, ours);
    return true;
}

bool PatchBytes(const char* name, std::uint32_t address, const std::uint8_t* expected,
                const std::uint8_t* replacement, std::uint32_t count) {
    // An operand inside Capcom's code, in the exe host: nothing to write. The
    // divergence keeps the value it writes as its own from this answer, and
    // our readers read that (docs/operand-store.md); .stub itself is never
    // written (the no-access trap, exe_host.cpp TrapStub). Outside the code range
    // (a .data byte, a switch of ours) the write is the same in both hosts.
    const bool code = InCode(address) || InCode(address + count - 1);
    const bool exe = IsExeHost() && code;
    if (exe && !(InCode(address) && InCode(address + count - 1)))
        Fatal("%s: %u bytes at 0x%08X straddle the end of Capcom's code", name, (unsigned)count, (unsigned)address);
    // `address` is a VA of the image - code, or a .data byte, which in the
    // exe host moves with the image: its place is Img(address) - or a switch
    // of ours, a byte of our DLL, which is its own place. Told apart by the
    // linked span 0x400000..kOwnVa, which nothing of ours lies in (the
    // executable holds it at 0x400000; the fence, at another base).
    const bool linked = address >= image::kBase && address < exe_layout::kOwnVa;
    auto* at = reinterpret_cast<std::uint8_t*>(static_cast<std::uintptr_t>(linked ? Img(address) : address));
    if (!exe && std::memcmp(at, expected, count) != 0)
        Fatal("%s: the %u bytes at 0x%08X are not the ones expected", name, (unsigned)count, (unsigned)address);
    if (const char* off = CodeOff(name)) {
        Log("patch   OFF  %-24s %u bytes at 0x%08X left alone (%s)", name, (unsigned)count, (unsigned)address, off);
        return false;
    }
    if (exe) {
        Log("patch   ON   %-24s %u bytes at 0x%08X (a value of ours; no code in this host)", name, (unsigned)count,
            (unsigned)address);
        return true;
    }
    DWORD old = 0;
    if (!VirtualProtect(at, count, PAGE_EXECUTE_READWRITE, &old))
        Fatal("%s: VirtualProtect(%p) failed, error %lu", name, (void*)at, GetLastError());
    std::memcpy(at, replacement, count);
    DWORD ignored = 0;
    VirtualProtect(at, count, old, &ignored);
    FlushInstructionCache(GetCurrentProcess(), at, count);
    Log("patch   ON   %-24s %u bytes at 0x%08X", name, (unsigned)count, (unsigned)address);
    return true;
}

void* CloneOriginal(const char* name, std::uint32_t original, std::uint32_t size,
                    const CloneCall* calls, int n_calls) {
    // RefuseHostedSwitches (exe_host.cpp) stops BOF3X_SHADOW before this can
    // be reached in the exe host; a clone asked for without it is a module
    // that clones outside the fuzz, and there is nothing to copy.
    if (IsExeHost()) Fatal("%s: CloneOriginal(0x%08X) in bof3x.exe, which maps no code of Capcom's to copy", name, (unsigned)original);
    auto* orig = reinterpret_cast<const std::uint8_t*>(static_cast<std::uintptr_t>(original));
    if (IsOwned(original)) Fatal("%s: CloneOriginal after Inject - the entry is already a jmp", name);
    // A tracer or debugger patch at the entry would be copied as a relative
    // jmp or a breakpoint (CC, int3), and run wrong from the new address. A function
    // whose first instruction is its own call (0x517200 is a list of calls)
    // starts with E8 legitimately, and one whose whole body is a tail jump
    // (0x595B30, five bytes, is Window_FreeState) starts with E9: both are
    // allowed when `calls` re-aims offset 0, which says the caller read that
    // byte in the disassembly as the original's own transfer - the copy then
    // holds a call or a jmp to the new target either way.
    bool entry_call_named = false;
    for (int i = 0; i < n_calls; ++i) entry_call_named = entry_call_named || calls[i].offset == 0;
    if (((orig[0] == 0xE9 || orig[0] == 0xE8) && !entry_call_named) || orig[0] == 0xCC)
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
        // Where the site reaches now. A site already re-aimed - by the tracer,
        // or by a RetargetCall that moved it to ours - would be copied as a
        // call to that code, so it is refused: against `expected` when the
        // caller says what the original called, and for "where the original
        // called" when it leaves the image. An entry Inject has patched (an
        // E9 there is ours; a BOF3X_ORIGINAL entry is not patched) is only
        // noted: it is an earlier module's function, already checked against
        // its own copy, and the same code on both sides of this comparison -
        // a module's own functions are not injected until its fuzz has run.
        const std::uint8_t* const callee = orig + at + kJmpLen + rel;
        const auto callee_va = static_cast<std::uint32_t>(reinterpret_cast<std::uintptr_t>(callee));
        if (calls[i].expected != 0 && callee_va != calls[i].expected)
            Fatal("%s: the call at +0x%X reaches 0x%08X, not 0x%08X: the site is re-aimed already, cannot clone", name,
                  (unsigned)at, (unsigned)callee_va, (unsigned)calls[i].expected);
        if (!calls[i].target) {
            if (callee_va < image::kBase || callee_va >= image::kBase + image::kSizeOfImage)
                Fatal("%s: the call at +0x%X leaves the image (0x%08X): re-aimed already, cannot clone", name,
                      (unsigned)at, (unsigned)callee_va);
            if (IsOwned(callee_va) && callee[0] == 0xE9)
                Log("shadow      %-24s the call at +0x%X reaches 0x%08X, which is ours: the same code on both sides",
                      name, (unsigned)at, (unsigned)callee_va);
        }
        const std::uint8_t* target = calls[i].target ? static_cast<const std::uint8_t*>(calls[i].target) : callee;
        rel = static_cast<std::int32_t>(target - (static_cast<std::uint8_t*>(copy) + at + kJmpLen));
        std::memcpy(static_cast<std::uint8_t*>(copy) + at + 1, &rel, sizeof rel);
    }
    FlushInstructionCache(GetCurrentProcess(), copy, size);
    Log("shadow      %-24s original 0x%08X cloned to %p, %u bytes", name, (unsigned)original, copy, (unsigned)size);
    return copy;
}

bool WantsShadow(const char* name) {
    char list[8192];  // a whole round of takeovers fits: 119 names is 2,061 characters
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

int OwnedCount() { return g_enabled + g_disabled; }

std::uint32_t OwnedAddress(int i) { return g_owned[i]; }

bool IsEnabled(std::uint32_t original) {
    for (int i = 0; i < g_enabled + g_disabled; ++i)
        if (g_owned[i] == original) return g_owned_on[i];
    return false;
}

void InjectReport() {
    Log("inject: %d ours, %d left original by BOF3X_ORIGINAL", g_enabled, g_disabled);
    int by_original = 0, by_off = 0;
    for (int i = 0; i < g_code_count; ++i) {
        if (!g_code[i].off) continue;
        if (std::strcmp(g_code[i].off, "BOF3X_ORIGINAL") == 0) ++by_original; else ++by_off;
    }
    Log("code names: %d asked for, %d off by BOF3X_ORIGINAL, %d off by BOF3X_OFF", g_code_count, by_original, by_off);
    // Every name BOF3X_OFF gives (bare or after `-`) must be a code name: a
    // misspelt name or an instrument hook would otherwise do nothing and say
    // nothing (CLAUDE.md rule 4), so it stops the game. A code name of
    // src/ (code_names.h) that this start did not ask for is a change not in
    // force under these settings - a language layout under another language,
    // the wide mode, the point filter, a cheat: logged, and the game runs.
    const char* list = OffList();
    if (*list) Log("BOF3X_OFF=%s", list);
    for (const char* p = list; *p;) {
        while (*p == ',' || *p == ' ') ++p;
        const char* end = p;
        while (*end && *end != ',' && *end != ' ') ++end;
        const char* q = p;
        if (q < end && *q == '-') ++q;
        const size_t n = static_cast<size_t>(end - q);
        if (end > p && !(end - p == 1 && *p == '*')) {
            bool asked = false;
            for (int i = 0; i < g_code_count && !asked; ++i)
                asked = std::strlen(g_code[i].name) == n && std::memcmp(g_code[i].name, q, n) == 0;
            if (!asked && n > 0 && KnownCodeName(q, n))
                Log("BOF3X_OFF: %.*s is not active in this start (its change is off with these settings: "
                    "the language, the wide mode, the filter or a cheat), so it has nothing to turn off",
                    (int)n, q);
            else if (!asked)
                Fatal("BOF3X_OFF names \"%.*s\", which is not a code name. "
                      "Use a code name from the ledger's Switch: lines (python tools/ledger_switches.py).",
                      (int)(end - p), p);
        }
        p = end;
    }
    Operands_Check("after InjectAll");
    Bind_Report();
}

}  // namespace bof3
