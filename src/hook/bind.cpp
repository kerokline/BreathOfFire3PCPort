#include "hook/bind.h"

#include <windows.h>

#include <algorithm>
#include <iterator>
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

// --- The places: one per VA ------------------------------------------------
//
// A code VA's place in bof3x.exe is not our function's address itself but a
// jump of ours to it, one per bound VA (and one for the thunk above), at
// (top << 24) | VA: the VA's own low three bytes under a top byte of ours. So
// every code word the game holds - loaded by the rebase, stored by our code,
// copied by the game's logic from one record to another - names exactly one
// VA, and reads as Capcom's word in all but its top byte: a byte or a low
// halfword the game's logic reads out of a code word (Capcom's code reads
// past its tables into the code-pointer table after them: EffectKind0B_Rise's
// s8 rise table 0x653AB0 at +9 >> 2 = 16 reads EffectKind0B_States[0]'s low
// byte, the stage C2 proof's whelpBoss, 2026-10-29), and a 24-bit link packed
// from one, are the VA's. The state hash reads a place back by its value
// (Bind_VaOf; statehash.cpp), exactly a bound VA under the top byte.
//
// The top byte is chosen by measurement: of the bytes 0x20..0x7F, the ones no
// aligned word of .data takes as (top << 24) | a bound VA, in the 227 raw dumps
// of analysis/statehash and the relocatable-image round's runs - 0x7C, 0x70,
// 0x7E, 0x7B and 0x7D. The first whose 0x1C3000 bytes the process can reserve
// is taken; if none, the first other top byte 0x20..0x7F that can be (logged:
// only the state hash's guarantee depends on the measurement); a Fatal only
// when no range at all can be reserved. The image itself, at 0x400000 or a
// developer's base (BOF3X_IMAGEBASE: 0x990000..0xA70000, below 16 MB), never
// reaches 0x20000000, so the table and the image cannot overlap. The stub page's jmps (mode S) were the same
// idea at top byte 0; these are in our memory, and .stub stays the trap.
constexpr std::uint8_t kTops[] = {0x7C, 0x70, 0x7E, 0x7B, 0x7D};
constexpr std::uint32_t kSpanLo = image::kCodeLo, kSpanHi = image::kCodeHi;
constexpr std::uint32_t kSpan = kSpanHi - kSpanLo;

std::uint32_t g_table = 0;   // (top << 24) once built (bof3x.exe), 0 hosted

std::uint32_t PlaceAt(std::uint32_t va) { return g_table | va; }

// Whether the table lies under one of the measured top bytes. Only the state
// hash cares (a data word under another top byte may read as a place and be
// renamed); the game runs the same under any.
bool g_measured = false;
unsigned g_refused = 0;   // top bytes tried that the process could not reserve

// The range (top << 24) | 0x401000 .. | 0x5C4000, reserved and committed
// read-write at that address, or null when something else holds part of it.
void* Reserve(std::uint32_t top) {
    const std::uint32_t lo = (top | kSpanLo) & ~0xFFFFu;
    void* const at = VirtualAlloc(reinterpret_cast<void*>(static_cast<std::uintptr_t>(lo)), (top | kSpanHi) - lo,
                                  MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE);
    if (at == reinterpret_cast<void*>(static_cast<std::uintptr_t>(lo))) return at;
    if (at) VirtualFree(at, 0, MEM_RELEASE);
    ++g_refused;
    return nullptr;
}

void BuildTable() {
    // The measured top bytes first; then any top byte 0x20..0x7F whose range
    // the process can reserve - a complete table either way, the game the
    // same, only the state hash's guarantee lost (logged by Bind_Report and the
    // state hash); a Fatal only when no range at all can be had.
    void* at = nullptr;
    std::uint32_t top = 0;
    for (const std::uint8_t b : kTops) {
        if ((at = Reserve(static_cast<std::uint32_t>(b) << 24)) != nullptr) {
            top = static_cast<std::uint32_t>(b) << 24;
            g_measured = true;
            break;
        }
    }
    for (std::uint32_t b = 0x20; !at && b < 0x80; ++b) {
        if (std::find(std::begin(kTops), std::end(kTops), b) != std::end(kTops)) continue;
        if ((at = Reserve(b << 24)) != nullptr) top = b << 24;
    }
    if (!at)
        Fatal("bind: cannot reserve the table of code places (0x%X bytes at (top << 24) | 0x%X) under any top byte "
              "0x20..0x7F, error %lu: the address space is full",
              (unsigned)kSpan, (unsigned)kSpanLo, GetLastError());
    auto* const t = reinterpret_cast<std::uint8_t*>(static_cast<std::uintptr_t>(top | kSpanLo));
    std::memset(t, 0xCC, kSpan);
    for (unsigned i = 0; i <= kRows; ++i) {
        const std::uint32_t va = i < kRows ? kBind[i].va : kDirectSoundThunk;
        const std::uint32_t to = i < kRows ? Key(kBind[i].ours) : Key(reinterpret_cast<const void*>(&DirectSoundThunk));
        const std::uint32_t from = top | va;
        auto* e = reinterpret_cast<std::uint8_t*>(static_cast<std::uintptr_t>(from));
        if (e[0] != 0xCC) Fatal("bind: the place of 0x%08X overlaps another's jump", (unsigned)va);
        const std::int32_t rel = static_cast<std::int32_t>(to - (from + 5));
        e[0] = 0xE9;
        std::memcpy(e + 1, &rel, 4);
    }
    DWORD old = 0;
    if (!VirtualProtect(at, kSpan + (kSpanLo & 0xFFFF), PAGE_EXECUTE_READ, &old))
        Fatal("bind: VirtualProtect of the table of code places failed, error %lu", GetLastError());
    FlushInstructionCache(GetCurrentProcess(), t, kSpan);
    g_table = top;
}

// The entry of a VA the table binds, or 0.
std::uint32_t PlaceOf(std::uint32_t va);

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
    if (mode == 2) BuildTable();
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

// Which rows an Inject named (bof3x.exe), for the report.
bool g_injected[kRows];
unsigned g_checked = 0;

std::uint32_t PlaceOf(std::uint32_t va) {
    if (Find(va) || va == kDirectSoundThunk) return PlaceAt(va);
    return 0;
}

}  // namespace

// --- common/image_base.h ----------------------------------------------------

std::uint32_t Code(std::uint32_t va) {
    if (Mode() == 1) return Img(va);
    if (const std::uint32_t place = PlaceOf(va)) return place;
    Miss(va);
    return Img(va);
}

std::uint32_t CodeToCall(std::uint32_t va) {
    if (Mode() == 1) return Img(va);
    if (const std::uint32_t place = PlaceOf(va)) return place;
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

std::uint32_t Bind_VaOf(std::uint32_t place) {
    if (g_table == 0 || (place & 0xFF000000u) != g_table) return 0;
    const std::uint32_t va = place & 0xFFFFFFu;
    return Find(va) || va == kDirectSoundThunk ? va : 0;
}

std::uint32_t Bind_VaOfFunction(std::uint32_t ours) {
    // the rows' indices by our address, sorted on first use (the game's thread)
    static std::uint16_t by_ours[kRows];
    static bool built = false;
    static_assert(kRows <= 0x10000, "16-bit row indices");
    if (!built) {
        for (unsigned i = 0; i < kRows; ++i) by_ours[i] = static_cast<std::uint16_t>(i);
        std::sort(by_ours, by_ours + kRows,
                  [](std::uint16_t a, std::uint16_t b) { return Key(kBind[a].ours) < Key(kBind[b].ours); });
        built = true;
    }
    const std::uint16_t* const begin = by_ours;
    const std::uint16_t* const end = by_ours + kRows;
    const std::uint16_t* p =
        std::lower_bound(begin, end, ours, [](std::uint16_t i, std::uint32_t v) { return Key(kBind[i].ours) < v; });
    return p != end && Key(kBind[*p].ours) == ours ? kBind[*p].va : 0;
}

bool Bind_TableMeasured() { return g_measured; }

std::uint32_t Bind_TableLo() { return g_table ? g_table | kSpanLo : 0; }
std::uint32_t Bind_TableHi() { return g_table ? g_table | kSpanHi : 0; }

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
    return r ? PlaceOf(va) : Img(va);
}

void Bind_Report() {
    if (Bind_Active() != IsExeHost())
        Fatal("bind: the bind table reads the host as %s, DetectHost as %s", Bind_Active() ? "bof3x.exe" : "BOF3.exe",
              IsExeHost() ? "bof3x.exe" : "BOF3.exe");
    if (!Bind_Active()) {
        Log("bind: hosted - nothing bound, a code VA is its own place in BOF3.exe (%u rows in the table, unused)", kRows);
    } else {
        Log("bind: bof3x.exe - %u functions bound by name (symbols.toml impl), %u of them named by an Inject; their "
            "places jumps at 0x%08X..0x%08X, one per VA under its low three bytes; .stub the trap; %u distinct VAs a table placed with "
            "no function of ours (%u times), each given the trap:",
            kRows, g_checked, (unsigned)Bind_TableLo(), (unsigned)Bind_TableHi(), g_misses, g_miss_calls);
        if (!g_measured)
            Log("bind: place table at 0x%08X: not a measured top byte (the five measured were taken: %u ranges "
                "refused), the state hash may rename data words",
                (unsigned)Bind_TableLo(), g_refused);
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
        // (the state hash's range, 0x5DA000..0x93E000) holding a code place,
        // by page - the pages whose hash the reverse map keeps equal to
        // Capcom's (statehash.cpp, flags bit 2).
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
        Log("bind: %u aligned words of .data hold a code place, on %u of its %u pages (from 0x%06X, "
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
        // our function's own address is not a code place: a value of the game's
        // data equal to it is data (the stage C2 proof, 2026-10-29)
        if (Bind_VaOf(Key(r.ours)) != 0) Fatal("bind self-test: our function %p reads as a code place", r.ours);
        std::uint32_t start = 0;
        const char* fn = Bind_FuncName(r.va, &start);
        if (!fn || start != r.va) Fatal("bind self-test: 0x%08X has no [[func]] of its own", (unsigned)r.va);
        const std::uint32_t expect = Bind_Active() ? PlaceAt(r.va) : Img(r.va);
        if (Code(r.va) != expect || CodeToCall(r.va) != expect || CodePlace(r.va) != expect)
            Fatal("bind self-test: 0x%08X places as 0x%08X, not 0x%08X", (unsigned)r.va, (unsigned)Code(r.va),
                  (unsigned)expect);
        if (CodePlace(expect) != expect) Fatal("bind self-test: the place of 0x%08X is placed again", (unsigned)r.va);
        if (IsCodeWord(expect) != true) Fatal("bind self-test: 0x%08X's place is not a code word", (unsigned)r.va);
        if (Bind_Active()) {
            // one place per VA, mapped back to that VA alone, its jmp reaching ours
            if (Bind_VaOf(expect) != r.va || Bind_VaOf(expect + 1) != 0 || Bind_VaOf(expect + 4) != 0 ||
                (expect & 0xFFFFFFu) != r.va)
                Fatal("bind self-test: the place 0x%08X does not map back to 0x%08X alone", (unsigned)expect, (unsigned)r.va);
            const auto* e = reinterpret_cast<const std::uint8_t*>(static_cast<std::uintptr_t>(expect));
            std::int32_t rel = 0;
            std::memcpy(&rel, e + 1, 4);
            if (e[0] != 0xE9 || expect + 5 + static_cast<std::uint32_t>(rel) != Key(r.ours))
                Fatal("bind self-test: the place 0x%08X does not jump to %p", (unsigned)expect, r.ours);
        }
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
        if (Code(kDirectSoundThunk) != PlaceAt(kDirectSoundThunk) || CodeToCall(kDirectSoundThunk) != PlaceAt(kDirectSoundThunk) ||
            Bind_VaOf(PlaceAt(kDirectSoundThunk)) != kDirectSoundThunk || !IsCodeWord(PlaceAt(kDirectSoundThunk)) || slot == 0 ||
            Bind_VaOf(Key(reinterpret_cast<const void*>(&DirectSoundThunk))) != 0)
            Fatal("bind self-test: DirectSoundCreate's thunk is not ours, or its slot 0x%08X is empty",
                  (unsigned)kDirectSoundSlot);
    } else if (IsCodeWord(Img(0x5DA000)) || !IsCodeWord(Img(image::kTextVa))) {
        Fatal("bind self-test: hosted, .data reads as code or .text does not");
    }
    Log("bind self-test: %u rows ascending, each looked up, placed (%s), mapped back and named; %u VAs between rows "
        "not bound%s; %u function names ascending, %u of them with no function of ours",
        kRows, Bind_Active() ? "as its own jump, the VA under a top byte" : "as itself, hosted", gaps,
        Bind_Active() ? ", their words the trap; DirectSoundCreate's thunk ours, through its bound slot" : "", kFuncs,
        unbound);
}

}  // namespace bof3
