#include "hook/bind.h"

#include <windows.h>

#include <algorithm>
#include <cstring>

#include "bof3/bind.gen.h"
#include "hook/exe_layout.h"
#include "hook/host.h"
#include "hook/log.h"

namespace bof3 {
namespace {

using bind_gen::kBind;
using bind_gen::kFunc;
using bind_gen::kFuncs;
using bind_gen::kRows;

std::uint32_t Key(const void* p) { return static_cast<std::uint32_t>(reinterpret_cast<std::uintptr_t>(p)); }

// DirectSoundCreate's import thunk, `jmp [0x5C4014]`, which Snd_Init calls
// as a function (sound_rest_callees.h kDirectSoundCreate) and which is not a
// [[func]]. Bound to DirectSoundThunk below, ours, which does what the thunk
// does: it calls through the slot as the slot reads at the call. A constant,
// so the namespace-scope callee table that holds it (sound_rest.cpp
// kOriginals, built before DllMain) holds a working function, and a later
// write of the slot - BindSlots filling it, wav_dump.cpp hooking it - is seen
// at the next call, as through Capcom's thunk.
constexpr std::uint32_t kDirectSoundThunk = 0x5ACBBC;
constexpr std::uint32_t kDirectSoundSlot = 0x5C4014;

using CreateFn = long(__stdcall*)(const void*, void**, void*);

long __stdcall DirectSoundThunk(const void* guid, void** out, void* outer) {
    CreateFn fn = nullptr;
    std::memcpy(&fn, reinterpret_cast<const void*>(static_cast<std::uintptr_t>(Img(kDirectSoundSlot))), sizeof fn);
    if (!fn)
        Fatal("bind: DirectSoundCreate's thunk 0x%08X called before its import slot 0x%08X is bound (exe_host.cpp "
              "BindSlots)",
              (unsigned)kDirectSoundThunk, (unsigned)kDirectSoundSlot);
    return fn(guid, out, outer);
}

std::uint32_t Thunk() { return Key(reinterpret_cast<const void*>(&DirectSoundThunk)); }

// 0: not read yet; 1: hosted, nothing bound; 2: bof3x.exe, bound. A plain
// int, zero before any initialiser runs, set on the first Code() - which may
// come from a namespace-scope table of another translation unit, before
// DllMain and before this file's own dynamic initialisers (it has none).
int g_mode = 0;

int Mode() {
    if (g_mode) return g_mode;
    // The main module's header, where ImageBase() says it is (the PEB's
    // ImageBaseAddress: mapped before any DLL initialises). bof3x.exe by its
    // timestamp, as DetectHost tells it (host.cpp); anything else is BOF3.exe
    // here, and DetectHost refuses a third module before the game starts.
    const auto* base = reinterpret_cast<const std::uint8_t*>(static_cast<std::uintptr_t>(ImageBase()));
    std::uint32_t lfanew = 0, stamp = 0;
    std::memcpy(&lfanew, base + 0x3C, 4);
    std::memcpy(&stamp, base + lfanew + 8, 4);
    const int mode = stamp == exe_layout::kTimestamp ? 2 : 1;
    // The table is static data the loader filled (bind.gen.h): a row that
    // reads null here would mean a dynamic initialiser, run after the tables
    // that need it.
    if (mode == 2 && (kBind[0].ours == nullptr || kBind[kRows - 1].ours == nullptr))
        Fatal("bind: the bind table is empty at its first use: it was not initialised statically");
    g_mode = mode;
    return mode;
}

const bind_gen::Row* Find(std::uint32_t va) {
    const bind_gen::Row* const end = kBind + kRows;
    const bind_gen::Row* r = std::lower_bound(kBind, end, va, [](const bind_gen::Row& x, std::uint32_t v) { return x.va < v; });
    return r != end && r->va == va ? r : nullptr;
}

// The VAs a table cell placed that the table does not name: the trap's, in
// the order first seen. Zero-initialised, filled from before DllMain.
constexpr unsigned kMissMax = 64;
std::uint32_t g_miss[kMissMax];
unsigned g_misses = 0, g_miss_calls = 0;

void Miss(std::uint32_t va) {
    ++g_miss_calls;
    for (unsigned i = 0; i < g_misses && i < kMissMax; ++i)
        if (g_miss[i] == va) return;
    if (g_misses < kMissMax) g_miss[g_misses] = va;
    ++g_misses;
}

// The reverse map: the rows' indices by our address. Built on first use, on
// the one thread that asks (the state hash, the dispatchers' IsCodeWord: the
// game's thread).
std::uint16_t g_by_ours[kRows];
bool g_reverse = false;
std::uint32_t g_ours_lo = 0, g_ours_hi = 0;

void BuildReverse() {
    static_assert(kRows <= 0x10000, "the reverse map's indices are 16-bit");
    for (unsigned i = 0; i < kRows; ++i) g_by_ours[i] = static_cast<std::uint16_t>(i);
    std::sort(g_by_ours, g_by_ours + kRows,
              [](std::uint16_t a, std::uint16_t b) { return Key(kBind[a].ours) < Key(kBind[b].ours); });
    for (unsigned i = 1; i < kRows; ++i)
        if (Key(kBind[g_by_ours[i]].ours) == Key(kBind[g_by_ours[i - 1]].ours))
            Fatal("bind: %s 0x%08X and 0x%08X are bound to the same function of ours %p", "the VAs",
                  (unsigned)kBind[g_by_ours[i - 1]].va, (unsigned)kBind[g_by_ours[i]].va, kBind[g_by_ours[i]].ours);
    g_ours_lo = Key(kBind[g_by_ours[0]].ours);
    g_ours_hi = Key(kBind[g_by_ours[kRows - 1]].ours);
    g_reverse = true;
}

// Which rows an Inject named (bof3x.exe), for the report.
bool g_injected[kRows];
unsigned g_checked = 0;

}  // namespace

// --- common/image_base.h ----------------------------------------------------

std::uint32_t Code(std::uint32_t va) {
    if (Mode() == 1) return Img(va);
    if (const bind_gen::Row* r = Find(va)) return Key(r->ours);
    if (va == kDirectSoundThunk) return Thunk();
    Miss(va);
    return Img(va);
}

std::uint32_t CodeToCall(std::uint32_t va) {
    if (Mode() == 1) return Img(va);
    if (const bind_gen::Row* r = Find(va)) return Key(r->ours);
    if (va == kDirectSoundThunk) return Thunk();
    std::uint32_t start = 0;
    const char* name = Bind_FuncName(va, &start);
    Fatal("bind: a call to 0x%08X (%s%s+0x%X), which no function of ours stands for in bof3x.exe: Capcom's code is "
          "not mapped here (the bind table, symbols.toml impl)",
          (unsigned)va, name ? name : "no [[func]]", "", (unsigned)(va - start));
}

bool IsCodeWord(std::uint32_t word) {
    if (Mode() == 1) return word >= image::kTextVa && word < image::kTextVa + image::kTextSize;
    return Bind_VaOf(word) != 0;
}

// --- bind.h -----------------------------------------------------------------

bool Bind_Active() { return Mode() == 2; }

unsigned Bind_Rows() { return kRows; }

std::uint32_t Bind_RowVa(unsigned i) {
    if (i >= kRows) Fatal("bind: row %u of %u", i, kRows);
    return kBind[i].va;
}

const void* Bind_Ours(std::uint32_t va) {
    const bind_gen::Row* r = Find(va);
    return r ? r->ours : nullptr;
}

std::uint32_t Bind_VaOf(std::uint32_t ours) {
    if (!g_reverse) BuildReverse();
    if (ours == Thunk()) return kDirectSoundThunk;
    if (ours < g_ours_lo || ours > g_ours_hi) return 0;
    const std::uint16_t* const begin = g_by_ours;
    const std::uint16_t* const end = g_by_ours + kRows;
    const std::uint16_t* p =
        std::lower_bound(begin, end, ours, [](std::uint16_t i, std::uint32_t v) { return Key(kBind[i].ours) < v; });
    return p != end && Key(kBind[*p].ours) == ours ? kBind[*p].va : 0;
}

const char* Bind_FuncName(std::uint32_t va, std::uint32_t* start) {
    if (va < image::kCodeLo || va >= image::kCodeHi) return nullptr;
    const bind_gen::Func* const end = kFunc + kFuncs;
    const bind_gen::Func* f = std::upper_bound(kFunc, end, va, [](std::uint32_t v, const bind_gen::Func& x) { return v < x.pc; });
    if (f == kFunc) return nullptr;
    --f;
    if (start) *start = f->pc;
    return f->name;
}

void Bind_Check(const char* name, std::uint32_t va, const void* ours) {
    const bind_gen::Row* r = Find(va);
    if (!r)
        Fatal("bind: %s is injected at 0x%08X, which the bind table does not name: a function of ours with no `impl` "
              "in symbols.toml (tools/gen_symbols.py)",
              name, (unsigned)va);
    if (r->ours != ours)
        Fatal("bind: %s at 0x%08X is injected as %p, but the bind table binds that VA to %p", name, (unsigned)va, ours,
              r->ours);
    if (!g_injected[r - kBind]) {
        g_injected[r - kBind] = true;
        ++g_checked;
    }
}

std::uint32_t Bind_Word(std::uint32_t va, bool* bound) {
    const bind_gen::Row* r = Find(va);
    *bound = r != nullptr;
    return r ? Key(r->ours) : Img(va);
}

void Bind_Report() {
    if (Bind_Active() != IsExeHost())
        Fatal("bind: the bind table reads the host as %s, DetectHost as %s", Bind_Active() ? "bof3x.exe" : "BOF3.exe",
              IsExeHost() ? "bof3x.exe" : "BOF3.exe");
    if (!Bind_Active()) {
        Log("bind: hosted - nothing bound, a code VA is its own place in BOF3.exe (%u rows in the table, the reverse "
            "map for the state hash)",
            kRows);
    } else {
        Log("bind: bof3x.exe - %u functions bound by name (symbols.toml impl), %u of them named by an Inject; .stub "
            "the trap; %u distinct VAs a table placed with no function of ours (%u times), each given the trap:",
            kRows, g_checked, g_misses, g_miss_calls);
        for (unsigned i = 0; i < g_misses && i < kMissMax; ++i) {
            std::uint32_t start = 0;
            const char* fn = Bind_FuncName(g_miss[i], &start);
            Log("bind:   0x%08X %s+0x%X", (unsigned)g_miss[i], fn ? fn : "(no [[func]])", (unsigned)(g_miss[i] - start));
        }
        if (g_misses > kMissMax) Log("bind:   and %u more", g_misses - kMissMax);
        unsigned listed = 0;
        for (unsigned i = 0; i < kRows; ++i) {
            if (g_injected[i]) continue;
            if (listed++ < 16) Log("bind:   bound, not injected in this start: 0x%08X", (unsigned)kBind[i].va);
        }
        if (listed > 16) Log("bind:   and %u more bound, not injected", listed - 16);
        // Where the bound words lie: every aligned word of .data and .bss
        // (the state hash's range, 0x5DA000..0x93E000) holding a bound
        // function's start, by page - the pages whose hash the reverse map
        // keeps equal to Capcom's (statehash.cpp, flags bit 2).
        constexpr std::uint32_t kLo = 0x5DA000, kHi = 0x93E000, kPage = 0x1000;
        unsigned words = 0, pages = 0, last = 0;
        std::uint32_t first_pages[8] = {};
        for (std::uint32_t va = kLo; va < kHi; va += 4) {
            std::uint32_t w;
            std::memcpy(&w, reinterpret_cast<const void*>(static_cast<std::uintptr_t>(Img(va))), 4);
            if (!Bind_VaOf(w)) continue;
            ++words;
            const unsigned page = (va - kLo) / kPage + 1;
            if (page != last) {
                if (pages < 8) first_pages[pages] = kLo + (page - 1) * kPage;
                ++pages;
                last = page;
            }
        }
        Log("bind: %u aligned words of .data hold a bound function of ours, on %u of its %u pages (from 0x%06X, "
            "0x%06X, 0x%06X, 0x%06X, ...)",
            words, pages, (kHi - kLo) / kPage, (unsigned)first_pages[0], (unsigned)first_pages[1],
            (unsigned)first_pages[2], (unsigned)first_pages[3]);
    }
    if (GetEnvironmentVariableA("BOF3X_SELFTEST_ONLY", nullptr, 0) > 0) Bind_SelfTest();
}

void Bind_SelfTest() {
    for (unsigned i = 1; i < kRows; ++i)
        if (kBind[i - 1].va >= kBind[i].va)
            Fatal("bind self-test: rows %u and %u are not ascending (0x%08X, 0x%08X)", i - 1, i, (unsigned)kBind[i - 1].va,
                  (unsigned)kBind[i].va);
    for (unsigned i = 1; i < kFuncs; ++i)
        if (kFunc[i - 1].pc >= kFunc[i].pc) Fatal("bind self-test: the function names are not ascending at %u", i);
    unsigned gaps = 0;
    for (unsigned i = 0; i < kRows; ++i) {
        const bind_gen::Row& r = kBind[i];
        if (r.va < image::kCodeLo || r.va >= image::kCodeHi)
            Fatal("bind self-test: row %u, 0x%08X, is outside the code range", i, (unsigned)r.va);
        if (Bind_Ours(r.va) != r.ours) Fatal("bind self-test: 0x%08X does not look up as its row", (unsigned)r.va);
        if (Bind_VaOf(Key(r.ours)) != r.va) Fatal("bind self-test: %p does not map back to 0x%08X", r.ours, (unsigned)r.va);
        if (Bind_VaOf(Key(r.ours) + 1) != 0) Fatal("bind self-test: %p + 1 maps back to a VA", r.ours);
        std::uint32_t start = 0;
        const char* fn = Bind_FuncName(r.va, &start);
        if (!fn || start != r.va) Fatal("bind self-test: 0x%08X has no [[func]] of its own", (unsigned)r.va);
        const std::uint32_t expect = Bind_Active() ? Key(r.ours) : Img(r.va);
        if (Code(r.va) != expect || CodeToCall(r.va) != expect || CodePlace(r.va) != expect)
            Fatal("bind self-test: 0x%08X places as 0x%08X, not 0x%08X", (unsigned)r.va, (unsigned)Code(r.va),
                  (unsigned)expect);
        if (CodePlace(Key(r.ours)) != Key(r.ours)) Fatal("bind self-test: the bound word %p is placed again", r.ours);
        if (IsCodeWord(expect) != true) Fatal("bind self-test: 0x%08X's place is not a code word", (unsigned)r.va);
        // a VA inside a function, between two rows: no function of ours
        if (i + 1 < kRows && kBind[i + 1].va - r.va > 1) {
            if (Bind_Ours(r.va + 1) != nullptr) Fatal("bind self-test: 0x%08X + 1 looks up", (unsigned)r.va);
            bool bound = true;
            if (Bind_Word(r.va + 1, &bound) != Img(r.va + 1) || bound)
                Fatal("bind self-test: the word 0x%08X + 1 is not the trap", (unsigned)r.va);
            ++gaps;
        }
    }
    if (Bind_Ours(kBind[kRows - 1].va + 1) != nullptr || Bind_Ours(image::kCodeLo - 1) != nullptr)
        Fatal("bind self-test: a VA outside the rows looks up");
    // a [[func]] without impl: named, not bound
    unsigned unbound = 0;
    for (unsigned i = 0; i < kFuncs; ++i)
        if (!Bind_Ours(kFunc[i].pc)) ++unbound;
    if (Bind_Active()) {
        // the data and the trap are not code words; the thunk is ours, mapped
        // back, and its slot is bound by now
        if (IsCodeWord(Img(0x5DA000)) || IsCodeWord(Img(kBind[0].va + 1)))
            Fatal("bind self-test: a data place or the trap reads as a code word");
        std::uint32_t slot = 0;
        std::memcpy(&slot, reinterpret_cast<const void*>(static_cast<std::uintptr_t>(Img(kDirectSoundSlot))), 4);
        if (Code(kDirectSoundThunk) != Thunk() || CodeToCall(kDirectSoundThunk) != Thunk() ||
            Bind_VaOf(Thunk()) != kDirectSoundThunk || !IsCodeWord(Thunk()) || slot == 0)
            Fatal("bind self-test: DirectSoundCreate's thunk is not ours, or its slot 0x%08X is empty",
                  (unsigned)kDirectSoundSlot);
    } else if (IsCodeWord(Img(0x5DA000)) || !IsCodeWord(Img(image::kTextVa))) {
        Fatal("bind self-test: hosted, .data reads as code or .text does not");
    }
    Log("bind self-test: %u rows ascending, each looked up, placed (%s), mapped back and named; %u VAs between rows "
        "not bound%s; %u function names ascending, %u of them with no function of ours",
        kRows, Bind_Active() ? "as our function" : "as itself, hosted", gaps,
        Bind_Active() ? ", their words the trap; DirectSoundCreate's thunk ours, through its bound slot" : "", kFuncs,
        unbound);
}

}  // namespace bof3
