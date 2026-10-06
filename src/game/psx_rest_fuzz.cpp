// BOF3X_SHADOW=psx_rest: a differential fuzz of the PSX library layer's last
// seventeen against byte-copies of Capcom's, once at start-up
// (docs/psx-rest.md section 4).
//
// The six leaves (three setters, the texture window, SquareRoot0,
// ApplyMatrixSV) run on a scratch buffer, arguments overlapping it and each
// other. SquareRoot0's copy keeps its tail jump to the C runtime's _ftol and
// runs under the game's control word 0x027F and under 0x037F, where it must
// agree, and under 0x007F, where it must NOT always agree - the standing proof
// that the fuzz sees the precision.
//
// The eleven of the teardown chain: every relative call out of a copy is
// re-aimed at a recording stand-in (Crt_free, D3d_FreeCellTexture,
// SndStream_Stop, Music_Release, and the teardown's seven), ours put on the
// same stand-ins through psx_rest::g; then the teardown as a tree, a copy
// calling copies of the seven against ours calling ours. The four Win32 calls
// go through the game's import slots: the copies' absolute operands are moved
// onto slots of ours holding recorders, and ours calls the recorders through
// g. DirectDraw, Direct3D and DirectSound are eight fake COM objects on one
// vtable - Release +0x08, GetDC +0x44, RestoreDisplayMode +0x4C,
// SetCooperativeLevel +0x50, ReleaseDC +0x68; any other slot ends the process
// naming itself - which record the object and arguments and answer from a
// hash. A recorder may, one call in four, disturb what its caller reads next:
// a COM slot swapped for another fake or nulled (never one the original reads
// again without a test), a texture-cache state byte, Gfx_RenderFlags bit
// 0x100, a byte of the text. This runs before BOF3.exe's C runtime is up: no
// real Crt_free, GDI or message box is reached.
//
// Compared per round: every region below, byte for byte, a hash of the whole
// log of calls with its length, and the leaves' results.
#include <cstdint>
#include <cstring>
#include <initializer_list>
#include <utility>

#include "bof3/symbols.gen.h"
#include "game/psx_rest_callees.h"
#include "hook/detour.h"
#include "hook/log.h"

namespace psx_rest {
namespace {

unsigned char* At(U address) { return reinterpret_cast<unsigned char*>(static_cast<std::uintptr_t>(address)); }
U Addr(const volatile void* p) { return static_cast<U>(reinterpret_cast<std::uintptr_t>(p)); }
U Long(U address) {
    U v;
    std::memcpy(&v, At(address), sizeof v);
    return v;
}
void PutLong(U address, U v) { std::memcpy(At(address), &v, sizeof v); }

// --- Random numbers of our own ----------------------------------------------
U g_rand = 0x5A6380A7u;
U Next() {
    U x = g_rand;
    x ^= x << 13;
    x ^= x >> 17;
    x ^= x << 5;
    return g_rand = x;
}

unsigned short ControlWord() {
    unsigned short cw;
    __asm__ volatile("fnstcw %0" : "=m"(cw));
    return cw;
}
void SetControlWord(unsigned short cw) { __asm__ volatile("fldcw %0" : : "m"(cw)); }

void* Clone(const char* name, U base, U size, std::initializer_list<bof3::CloneCall> calls) {
    return bof3::CloneOriginal(name, base, size, calls.begin(), static_cast<int>(calls.size()));
}
const void* F(auto fn) { return reinterpret_cast<const void*>(fn); }

// =============================================================================
// The six leaves
// =============================================================================

unsigned g_leaf_bad;
void LeafMismatch(const char* name, unsigned round, U a, U b) {
    if (++g_leaf_bad <= 8)
        bof3::Log("shadow      psx_rest self-test MISMATCH: %s, round %u, %08X / %08X", name, round, (unsigned)a, (unsigned)b);
}

using PrimFn = void (__cdecl*)(unsigned char*, U);
void FuzzPrim(const char* name, const void* theirs, const void* ours, unsigned rounds) {
    static unsigned char input[0x60], a[0x60], b[0x60];
    for (unsigned round = 0; round < rounds; ++round) {
        for (auto& x : input) x = static_cast<unsigned char>(Next());
        const U at = Next() % 0x10;
        // The texture window's rect is only stored, never read: any value.
        const U rect = Next() % 4 == 0 ? Addr(a + Next() % 0x40) : Next();
        std::memcpy(a, input, sizeof a);
        reinterpret_cast<PrimFn>(const_cast<void*>(theirs))(a + at, rect);
        std::memcpy(b, a, sizeof b);
        std::memcpy(a, input, sizeof a);
        // ours on the same buffer, so that a stored address compares equal
        reinterpret_cast<PrimFn>(const_cast<void*>(ours))(a + at, rect);
        if (std::memcmp(a, b, sizeof a) != 0) {
            unsigned i = 0;
            while (a[i] == b[i]) ++i;
            LeafMismatch(name, round, i, (unsigned)b[i] << 8 | a[i]);
        }
    }
}

// SquareRoot0: exact squares and their neighbours, the edges, and noise -
// under three control words.
unsigned g_sqrt_differ_007f;
void FuzzSqrt(const void* theirs) {
    using Fn = long (__cdecl*)(long);
    const Fn t = reinterpret_cast<Fn>(const_cast<void*>(theirs));
    const unsigned short words[] = {0x027F, 0x037F, 0x007F};
    const unsigned short saved = ControlWord();
    static const long kEdges[] = {0, 1, 2, 3, 4, -1, -2, 0x7FFFFFFF, static_cast<long>(0x80000000u), 0x7FFEA810,
                                  0x7FFEA80F, 0x3FFFFFFF, 0x40000000, 0xFFFF, 0x10000};
    for (unsigned round = 0; round < 0x18000; ++round) {
        long n;
        switch (Next() % 6) {
        case 0: n = kEdges[Next() % (sizeof kEdges / sizeof kEdges[0])]; break;
        case 1: case 2: case 3: {
            const long k = static_cast<long>(Next() % 46341);
            n = k * k + static_cast<long>(Next() % 3) - 1;
            break;
        }
        case 4: n = static_cast<long>(Next() & 0xFFFFF); break;
        default: n = static_cast<long>(Next()); break;
        }
        const unsigned short cw = words[round % 3];
        SetControlWord(cw);
        const long a = t(n);
        SetControlWord(saved);
        const long b = Gte_SquareRoot0(n);
        if (a == b) continue;
        if (cw == 0x007F) ++g_sqrt_differ_007f;
        else LeafMismatch("Gte_SquareRoot0", round, static_cast<U>(a), static_cast<U>(b));
    }
}

// ApplyMatrixSV: matrix, vector and out anywhere in one buffer, overlapping as
// often as not; components from the edges a third of the time.
void FuzzApply(const void* theirs) {
    using Fn = void (__cdecl*)(const short*, const short*, short*);
    const Fn t = reinterpret_cast<Fn>(const_cast<void*>(theirs));
    static short input[0x30], a[0x30], b[0x30];
    static const short kEdges[] = {0, 1, -1, 0x7FFF, -0x8000, 0x1000, -0x1000, 0x0FFF};
    unsigned overlapping = 0;
    for (unsigned round = 0; round < 12000; ++round) {
        for (auto& x : input)
            x = Next() % 3 == 0 ? kEdges[Next() % 8] : static_cast<short>(Next());
        const U m = Next() % 0x20, v = Next() % 0x2C, o = Next() % 4 == 0 ? (Next() % 2 ? m + Next() % 9 : v + Next() % 3)
                                                                          : Next() % 0x2D;
        overlapping += (o + 3 > m && o < m + 9) || (o + 3 > v && o < v + 3);
        std::memcpy(a, input, sizeof a);
        t(a + m, a + v, a + o);
        std::memcpy(b, input, sizeof b);
        Gte_ApplyMatrixSV(b + m, b + v, b + o);
        if (std::memcmp(a, b, sizeof a) != 0) LeafMismatch("Gte_ApplyMatrixSV", round, m << 16 | v, o);
    }
    bof3::Log("shadow      psx_rest self-test: Gte_ApplyMatrixSV 12000 rounds, %u with out overlapping an input", overlapping);
}

// =============================================================================
// The teardown chain
// =============================================================================

// --- The memory a round compares ---------------------------------------------
constexpr unsigned kTextBytes = 0x48;
unsigned char g_text[kTextBytes];   // Display_TextOut's string

struct Region { U at, bytes; };
constexpr Region kRegions[] = {
    {kClutRows - 0x10, 0x1010},             // Gfx_ClutRows and the 0x10 bytes below
    {0x6C3A40, 0x10},                       // to Gfx_RenderFlags
    {kTexCache - 0x10, 0x6020},             // Gfx_TexCache
    {kFontCache - 0x10, 0xA20},             // Font_TexCache
    {0x7CADE0, 0x60},                       // the after-draw block and the orphans, to D3d_CellTexCache
    {kBackdropBlock - 8, 0x28},             // the backdrop block
    {0x7CC330, 0x30},                       // the DirectX 6 slots
    {0x7DE3B8, 0x10},                       // Snd_Device, Snd_PrimaryBuffer
};
constexpr unsigned SumRegions() {
    unsigned n = 0;
    for (const Region& r : kRegions) n += r.bytes;
    return n;
}
constexpr unsigned kStateBytes = SumRegions() + kTextBytes;

struct State {
    unsigned char bytes[kStateBytes];
    U log_hash, log_n;
    U first[6];   // the first call, for the log line
};
U g_log_hash, g_log_n, g_first[6];
U g_seed;

void Capture(State& s) {
    unsigned at = 0;
    for (const Region& r : kRegions) {
        std::memcpy(s.bytes + at, At(r.at), r.bytes);
        at += r.bytes;
    }
    std::memcpy(s.bytes + at, g_text, kTextBytes);
    s.log_hash = g_log_hash;
    s.log_n = g_log_n;
    std::memcpy(s.first, g_first, sizeof s.first);
}
void Apply(const State& s) {
    unsigned at = 0;
    for (const Region& r : kRegions) {
        std::memcpy(At(r.at), s.bytes + at, r.bytes);
        at += r.bytes;
    }
    std::memcpy(g_text, s.bytes + at, kTextBytes);
    g_log_hash = 0x811C9DC5u;
    g_log_n = 0;
    std::memset(g_first, 0, sizeof g_first);
}
U FirstDifference(const State& a, const State& b) {
    unsigned at = 0;
    for (const Region& r : kRegions) {
        for (U k = 0; k < r.bytes; ++k)
            if (a.bytes[at + k] != b.bytes[at + k]) return r.at + k;
        at += r.bytes;
    }
    for (U k = 0; k < kTextBytes; ++k)
        if (a.bytes[at + k] != b.bytes[at + k]) return Addr(g_text) + k;
    return 0;
}

U Hash(U salt) {
    U h = (g_seed + g_log_n * 0x632BE5ABu + salt * 0x2545F491u) * 0x9E3779B1u;
    h ^= h >> 15;
    h *= 0x85EBCA6Bu;
    h ^= h >> 13;
    h *= 0xC2B2AE35u;
    h ^= h >> 16;
    return h;
}
unsigned g_counts[0x40];
void Record(U what, U a = 0, U b = 0, U c = 0, U d = 0, U e = 0) {
    if (g_log_n == 0) {
        const U first[6] = {what, a, b, c, d, e};
        std::memcpy(g_first, first, sizeof g_first);
    }
    for (U v : {what, a, b, c, d, e}) g_log_hash = (g_log_hash ^ v) * 16777619u;
    ++g_log_n;
}

// --- The COM objects, faked --------------------------------------------------
struct Fake { const void* const* vtable; U id; };
constexpr unsigned kFakes = 8;
Fake g_fakes[kFakes];

U Id(const void* self) {
    for (unsigned i = 0; i < kFakes; ++i)
        if (self == &g_fakes[i]) return 1 + i;
    bof3::Fatal("psx_rest: a COM method was called on %p, which is no fake", self);
}
U FakeAddr(U v) { return Addr(&g_fakes[v % kFakes]); }

// What a recorder may change under its caller: the round's hot cells.
enum HotKind : U { kNullable, kNeverNull, kStateByte, kTextByte, kFlags, kPlain };
struct Hot { U at; HotKind kind; };
constexpr unsigned kMaxHot = 512;
Hot g_hot[kMaxHot];
unsigned g_n_hot;
void AddHot(U at, HotKind kind) {
    if (g_n_hot < kMaxHot) g_hot[g_n_hot++] = {at, kind};
}

unsigned g_disturbed;
void Disturb() {
    const U h = Hash(0xD157);
    if ((h & 0x300) || !g_n_hot) return;   // one call in four
    ++g_disturbed;
    const U v = h >> 12;
    const Hot& c = g_hot[(h >> 4) % g_n_hot];
    switch (c.kind) {
    case kNullable: PutLong(c.at, v % 3 == 0 ? 0 : FakeAddr(v >> 2)); break;
    case kNeverNull: PutLong(c.at, FakeAddr(v >> 2)); break;
    case kStateByte: At(c.at)[0] = (v & 1) ? static_cast<unsigned char>(v >> 1) : 0; break;
    case kTextByte: At(c.at)[0] = (v & 3) ? static_cast<unsigned char>(0x20 + (v >> 2) % 0x5F) : 0; break;
    case kFlags: PutLong(c.at, Long(c.at) ^ 0x100); break;
    default: PutLong(c.at, (v & 7) ? v * 0x9E3779B1u : 0); break;
    }
}

long __stdcall FakeRelease(void* self) {
    Record(0x08, Id(self));
    ++g_counts[0x08 / 4];
    Disturb();
    return static_cast<long>(Hash(0x4E1) & 0xFF);
}
long __stdcall FakeGetDC(void* self, void** hdc) {
    Record(0x44, Id(self));
    ++g_counts[0x44 / 4];
    const U h = Hash(0x6DC);
    *hdc = At(h | 0x10000);   // a handle: written always, used only on success
    Disturb();
    return h % 4 == 0 ? static_cast<long>(0x887601C2u) : h % 7 == 0 ? static_cast<long>(h) : 0;
}
long __stdcall FakeRestoreDisplayMode(void* self) {
    Record(0x4C, Id(self));
    ++g_counts[0x4C / 4];
    Disturb();
    return static_cast<long>(Hash(0x4D0));
}
long __stdcall FakeSetCooperativeLevel(void* self, void* hwnd, unsigned long flags) {
    Record(0x50, Id(self), Addr(hwnd), flags);
    ++g_counts[0x50 / 4];
    Disturb();
    return static_cast<long>(Hash(0xC00));
}
long __stdcall FakeReleaseDC(void* self, void* hdc) {
    Record(0x68, Id(self), Addr(hdc));
    ++g_counts[0x68 / 4];
    Disturb();
    return static_cast<long>(Hash(0x6EDC));
}
template <int S> long __stdcall FakeUnexpected(void* self) {
    bof3::Fatal("psx_rest: self-test reached COM slot %d (+0x%X) of object %u, which nothing fakes", S, S * 4,
                (unsigned)Id(self));
}

constexpr int kSlots = 32;
const void* g_vtable[kSlots];
template <int S> const void* Slot() {
    switch (S * 4) {
    case kRelease: return F(&FakeRelease);
    case kGetDC: return F(&FakeGetDC);
    case kRestoreDisplayMode: return F(&FakeRestoreDisplayMode);
    case kSetCooperativeLevel: return F(&FakeSetCooperativeLevel);
    case kReleaseDC: return F(&FakeReleaseDC);
    default: return F(&FakeUnexpected<S>);
    }
}
template <int... S> void FillVtable(std::integer_sequence<int, S...>) { ((g_vtable[S] = Slot<S>()), ...); }

void BuildFakes() {
    FillVtable(std::make_integer_sequence<int, kSlots>{});
    for (unsigned i = 0; i < kFakes; ++i) g_fakes[i] = {g_vtable, 1 + i};
}

// --- The stand-ins for the relative calls and the imports ----------------------
void __cdecl StubFree(void* p) {
    Record(0x100, Addr(p));
    Disturb();
}
void __cdecl StubFreeCell(int slot) {
    Record(0x101, static_cast<U>(slot));
    Disturb();
}
void __cdecl StubStreamStop() {
    Record(0x102);
    Disturb();
}
void __cdecl StubMusicRelease() {
    Record(0x103);
    Disturb();
}
template <U kWhat> void __cdecl StubSeven() {
    Record(kWhat);
    Disturb();
}
unsigned long __stdcall StubSetTextColor(void* hdc, unsigned long color) {
    Record(0x110, Addr(hdc), color);
    Disturb();
    return Hash(0x7C0);
}
int __stdcall StubSetBkMode(void* hdc, int mode) {
    Record(0x111, Addr(hdc), static_cast<U>(mode));
    Disturb();
    return static_cast<int>(Hash(0xB4D));
}
U Bytes(const void* p, unsigned n) {
    U h = 2166136261u;
    for (unsigned i = 0; i < n; ++i) h = (h ^ static_cast<const unsigned char*>(p)[i]) * 16777619u;
    return h;
}
int __stdcall StubTextOutA(void* hdc, int x, int y, const char* text, int n) {
    Record(0x112, Addr(hdc), static_cast<U>(x), static_cast<U>(y), Addr(text),
           static_cast<U>(n) ^ (n >= 0 && n <= static_cast<int>(kTextBytes) ? Bytes(text, static_cast<unsigned>(n)) : 0));
    Disturb();
    return static_cast<int>(Hash(0x7E7));
}
int __stdcall StubMessageBoxA(void* hwnd, const char* text, const char* caption, unsigned type) {
    // The text is a pointer read from the image's table: compared by value,
    // never followed (an out-of-table code reads whatever lies there).
    Record(0x113, Addr(hwnd), Addr(text), Addr(caption), type);
    Disturb();
    return static_cast<int>(Hash(0x3B0));
}

const Callees kStubs = {
    StubSeven<0x120>, StubSeven<0x121>, StubSeven<0x122>, StubSeven<0x123>, StubSeven<0x124>, StubSeven<0x125>,
    StubSeven<0x126>,
    StubFree, StubFreeCell, StubStreamStop, StubMusicRelease,
    StubSetTextColor, StubSetBkMode, StubTextOutA, StubMessageBoxA,
};
const Callees kTree = {
    Gfx_FreeClutRows, Gfx_ReleaseTexCache, Font_ReleaseTexCache, Display_ReleaseOrphans, Display_ReleaseBackdrop,
    D3d_FreeCellTextures, D3d_ReleaseAfterDraw,
    StubFree, StubFreeCell, StubStreamStop, StubMusicRelease,
    StubSetTextColor, StubSetBkMode, StubTextOutA, StubMessageBoxA,
};

// The copies' import slots: their absolute operands moved here.
const void* g_slot_set_text_color = F(&StubSetTextColor);
const void* g_slot_set_bk_mode = F(&StubSetBkMode);
const void* g_slot_text_out = F(&StubTextOutA);
const void* g_slot_message_box = F(&StubMessageBoxA);

void PatchImport(const char* name, void* copy, U offset, U slot, const void* const* ours) {
    auto* code = static_cast<unsigned char*>(copy);
    U disp;
    std::memcpy(&disp, code + offset, sizeof disp);
    if (code[offset - 2] != 0xFF || code[offset - 1] != 0x15 || disp != slot)
        bof3::Fatal("psx_rest: %s +0x%X is not a call through the import slot 0x%X", name, (unsigned)offset, (unsigned)slot);
    disp = Addr(ours);
    std::memcpy(code + offset, &disp, sizeof disp);
}

// --- The copies, by capstone 2026-10-05 -----------------------------------------
enum Kind : unsigned {
    kFreeClutRows, kReleaseTexCache, kReleaseFont, kReleaseOrphans, kReleaseBackdrop, kFreeCells, kReleaseAfterDraw,
    kTeardown, kTeardownTree, kTextOut, kErrorBox, kSoundShutdown, kKinds
};
const char* const kNames[kKinds] = {"Gfx_FreeClutRows", "Gfx_ReleaseTexCache", "Font_ReleaseTexCache",
                                    "Display_ReleaseOrphans", "Display_ReleaseBackdrop", "D3d_FreeCellTextures",
                                    "D3d_ReleaseAfterDraw", "Display_Teardown", "Display_Teardown tree",
                                    "Display_TextOut", "Display_ErrorBox", "Sound_Shutdown"};
void* g_clones[kKinds];

// The teardown's seven calls, by offset, and their callees.
struct Seven { U offset, callee; };
constexpr Seven kSeven[7] = {{0x01, 0x5A64B0}, {0x06, 0x5A64E0}, {0x0B, 0x5A6540}, {0x10, 0x5A6590},
                             {0x15, 0x5A65F0}, {0x1A, 0x5A6760}, {0x1F, 0x5A6650}};

void* CloneSevenOne(unsigned i) {
    switch (i) {
    case 0: return Clone("Gfx_FreeClutRows", 0x5A64B0, 0x2C, {{0xD, F(&StubFree), 0x5B9577}});
    case 1: return Clone("Gfx_ReleaseTexCache", 0x5A64E0, 0x55, {});
    case 2: return Clone("Font_ReleaseTexCache", 0x5A6540, 0x46, {});
    case 3: return Clone("Display_ReleaseOrphans", 0x5A6590, 0x5F, {});
    case 4: return Clone("Display_ReleaseBackdrop", 0x5A65F0, 0x55, {});
    case 5: return Clone("D3d_FreeCellTextures", 0x5A6760, 0x17, {{0x4, F(&StubFreeCell), 0x5A3790}});
    default: return Clone("D3d_ReleaseAfterDraw", 0x5A6650, 0x3E, {});
    }
}

void CloneAll() {
    for (unsigned i = 0; i < 7; ++i) g_clones[i] = CloneSevenOne(i);
    const void* const stubs[7] = {F(&StubSeven<0x120>), F(&StubSeven<0x121>), F(&StubSeven<0x122>),
                                  F(&StubSeven<0x123>), F(&StubSeven<0x124>), F(&StubSeven<0x125>),
                                  F(&StubSeven<0x126>)};
    g_clones[kTeardown] = Clone("Display_Teardown", 0x5A6380, 0x122,
                                {{kSeven[0].offset, stubs[0], kSeven[0].callee},
                                 {kSeven[1].offset, stubs[1], kSeven[1].callee},
                                 {kSeven[2].offset, stubs[2], kSeven[2].callee},
                                 {kSeven[3].offset, stubs[3], kSeven[3].callee},
                                 {kSeven[4].offset, stubs[4], kSeven[4].callee},
                                 {kSeven[5].offset, stubs[5], kSeven[5].callee},
                                 {kSeven[6].offset, stubs[6], kSeven[6].callee}});
    void* tree[7];
    for (unsigned i = 0; i < 7; ++i) tree[i] = CloneSevenOne(i);
    g_clones[kTeardownTree] = Clone("Display_Teardown", 0x5A6380, 0x122,
                                    {{kSeven[0].offset, tree[0], kSeven[0].callee},
                                     {kSeven[1].offset, tree[1], kSeven[1].callee},
                                     {kSeven[2].offset, tree[2], kSeven[2].callee},
                                     {kSeven[3].offset, tree[3], kSeven[3].callee},
                                     {kSeven[4].offset, tree[4], kSeven[4].callee},
                                     {kSeven[5].offset, tree[5], kSeven[5].callee},
                                     {kSeven[6].offset, tree[6], kSeven[6].callee}});
    g_clones[kTextOut] = Clone("Display_TextOut", 0x5A66B0, 0x6D, {});
    PatchImport("Display_TextOut", g_clones[kTextOut], 0x22, kImportSetTextColor, &g_slot_set_text_color);
    PatchImport("Display_TextOut", g_clones[kTextOut], 0x2F, kImportSetBkMode, &g_slot_set_bk_mode);
    PatchImport("Display_TextOut", g_clones[kTextOut], 0x56, kImportTextOutA, &g_slot_text_out);
    g_clones[kErrorBox] = Clone("Display_ErrorBox", 0x5A6720, 0x39, {});
    PatchImport("Display_ErrorBox", g_clones[kErrorBox], 0x1C, kImportMessageBoxA, &g_slot_message_box);
    PatchImport("Display_ErrorBox", g_clones[kErrorBox], 0x34, kImportMessageBoxA, &g_slot_message_box);
    g_clones[kSoundShutdown] = Clone("Sound_Shutdown", 0x5A6980, 0x3D,
                                     {{0x0, F(&StubStreamStop), 0x5A71C0}, {0x5, F(&StubMusicRelease), 0x5A70A0}});
}

const void* Ours(unsigned k) {
    switch (k) {
    case kFreeClutRows: return F(&Gfx_FreeClutRows);
    case kReleaseTexCache: return F(&Gfx_ReleaseTexCache);
    case kReleaseFont: return F(&Font_ReleaseTexCache);
    case kReleaseOrphans: return F(&Display_ReleaseOrphans);
    case kReleaseBackdrop: return F(&Display_ReleaseBackdrop);
    case kFreeCells: return F(&D3d_FreeCellTextures);
    case kReleaseAfterDraw: return F(&D3d_ReleaseAfterDraw);
    case kTeardown: case kTeardownTree: return F(&Display_Teardown);
    case kTextOut: return F(&Display_TextOut);
    case kErrorBox: return F(&Display_ErrorBox);
    case kSoundShutdown: return F(&Sound_Shutdown);
    default: bof3::Fatal("psx_rest: no function for kind %u", k);
    }
}

struct Args { U a[3]; };
void Run(const void* fn, unsigned k, const Args& x) {
    void* const p = const_cast<void*>(fn);
    switch (k) {
    case kTeardown: case kTeardownTree: case kErrorBox:
        reinterpret_cast<void (__cdecl*)(U)>(p)(x.a[0]);
        break;
    case kTextOut: reinterpret_cast<void (__cdecl*)(U, U, U)>(p)(x.a[0], x.a[1], x.a[2]); break;
    default: reinterpret_cast<void (__cdecl*)()>(p)(); break;
    }
}

// --- A round's input --------------------------------------------------------------
U RandomCom(unsigned odds_null) { return Next() % odds_null == 0 ? 0 : FakeAddr(Next()); }

void FillClutRows() {
    for (U at = kClutRows; at < kClutRowsEnd; at += 8) {
        PutLong(at, Next());
        PutLong(at + 4, Next() % 3 == 0 ? 0 : Next() | 1);
        if (Next() % 8 == 0) AddHot(at + 4, kPlain);
    }
}
// Each page live to a prefix: mostly short, sometimes all 32, sometimes none.
void FillTexCache() {
    for (U page = kTexCache; page < kTexCacheEnd; page += kTexPageBytes) {
        const U r = Next() % 16;
        const U live = r == 0 ? 0 : r == 1 ? 32 : r < 4 ? Next() % 33 : Next() % 4;
        for (U i = 0; i < 0x20; ++i) {
            const U entry = page + i * kTexEntryBytes;
            for (U k = 0; k < kTexEntryBytes; k += 4) PutLong(entry + k, Next());
            At(entry)[0] = i < live ? static_cast<unsigned char>(1 + Next() % 255) : (i == live ? 0 : At(entry)[0]);
            PutLong(entry + 0x10, RandomCom(3));
            PutLong(entry + 0x14, RandomCom(3));
            if (i <= live && Next() % 4 == 0) {
                AddHot(entry + 0x10, kNullable);
                AddHot(entry + 0x14, kNullable);
                AddHot(entry + (Next() % 2 ? kTexEntryBytes : 0), kStateByte);
            }
        }
    }
}
void FillFontCache() {
    for (U entry = kFontCache; entry < kFontCacheEnd; entry += kFontEntryBytes) {
        for (U k = 0; k < kFontEntryBytes; k += 4) PutLong(entry + k, Next());
        PutLong(entry + 8, RandomCom(3));
        PutLong(entry + 0xC, RandomCom(3));
        if (Next() % 4 == 0) AddHot(entry + 8 + (Next() % 2) * 4, kNullable);
    }
}
void FillSmall() {
    for (U at = 0x7CADE0; at < 0x7CAE40; at += 4) PutLong(at, Next());
    for (U at : {kAfterDrawSurface, kAfterDrawTexSurface, kAfterDrawTexture}) {
        PutLong(at, RandomCom(3));
        AddHot(at, kNullable);
    }
    for (U at = kOrphans; at < kOrphans + 0x18; at += 4) {
        PutLong(at, RandomCom(3));
        AddHot(at, kNullable);
    }
    for (U at = kBackdropBlock - 8; at < kBackdropBlock + 0x20; at += 4) PutLong(at, Next());
    for (U at = kBackdrop; at < kBackdrop + 0x14; at += 4) {
        PutLong(at, RandomCom(3));
        AddHot(at, kNullable);
    }
}
void FillSlots() {
    for (U at = 0x7CC330; at < 0x7CC360; at += 4) PutLong(at, Next());
    for (U at : {kBackMaterial, kViewport, kDevice, kDirect3D, kClipper, kZBuffer, kStage, kBackBuffer, kPrimary}) {
        PutLong(at, RandomCom(4));
        AddHot(at, kNullable);
    }
    PutLong(kDirectDraw, RandomCom(5));
    AddHot(kDirectDraw, kNeverNull);   // read again without a test after SetCooperativeLevel
    U flags = Next() & ~0x100u;
    if (Next() & 1) flags |= 0x100;
    PutLong(kRenderFlags, flags);
    AddHot(kRenderFlags, kFlags);
}
void FillSound() {
    for (U at = 0x7DE3B8; at < 0x7DE3C8; at += 4) PutLong(at, Next());
    for (U at : {kSndDevice, kSndPrimary}) {
        PutLong(at, RandomCom(3));
        AddHot(at, kNullable);
    }
}

Args Generate(unsigned k, unsigned* coverage) {
    g_n_hot = 0;
    for (auto& b : g_text) b = static_cast<unsigned char>(Next());
    Args x{};
    switch (k) {
    case kFreeClutRows: FillClutRows(); break;
    case kReleaseTexCache: FillTexCache(); break;
    case kReleaseFont: FillFontCache(); break;
    case kReleaseOrphans: case kReleaseBackdrop: case kReleaseAfterDraw: FillSmall(); break;
    case kFreeCells: FillSmall(); break;
    case kTeardown: case kTeardownTree:
        if (k == kTeardownTree) {
            FillClutRows();
            FillTexCache();
            FillFontCache();
        }
        FillSmall();
        FillSlots();
        FillSound();
        coverage[0] += (Long(kRenderFlags) & 0x100) && Long(kDirectDraw);
        x.a[0] = Next();
        break;
    case kTextOut: {
        FillSlots();
        PutLong(kBackBuffer, FakeAddr(Next()));
        g_n_hot = 0;
        AddHot(kBackBuffer, kNeverNull);
        const U end = Next() % (kTextBytes - 1);
        for (U i = 0; i < kTextBytes; ++i) {
            if (i < end) g_text[i] = static_cast<unsigned char>(1 + Next() % 255);
            else if (i == end || i == kTextBytes - 1) g_text[i] = 0;
            if (i < kTextBytes - 1 && Next() % 6 == 0) AddHot(Addr(g_text + i), kTextByte);
        }
        x.a[0] = Next() % 3 == 0 ? Next() % 640 : Next();
        x.a[1] = Next() % 3 == 0 ? Next() % 480 : Next();
        x.a[2] = Addr(g_text);
        break;
    }
    case kErrorBox: {
        // Every entry of both tables, the boundary either side, and a few
        // below the first: what the image holds there is passed, not read.
        const U r = Next() % 8;
        const int code = r < 4 ? static_cast<int>(Next() % 20)
                         : r < 6 ? 100 + static_cast<int>(Next() % 4)
                         : r == 6 ? 96 + static_cast<int>(Next() % 8)
                                  : -static_cast<int>(1 + Next() % 8);
        coverage[1] += code >= 100;
        x.a[0] = static_cast<U>(code);
        break;
    }
    case kSoundShutdown: FillSound(); break;
    default: break;
    }
    return x;
}

}  // namespace

void SelfTest() {
    // --- the leaves ---
    g_leaf_bad = 0;
    FuzzPrim("Gpu_SetPolyF3", Clone("Gpu_SetPolyF3", 0x5A7570, 0x17, {}), F(&Gpu_SetPolyF3), 3000);
    FuzzPrim("Gpu_SetPolyFT3", Clone("Gpu_SetPolyFT3", 0x5A7590, 0x17, {}), F(&Gpu_SetPolyFT3), 3000);
    FuzzPrim("Gpu_SetLineG4", Clone("Gpu_SetLineG4", 0x5A76F0, 0x1A, {}), F(&Gpu_SetLineG4), 3000);
    FuzzPrim("Gpu_SetTexWindow", Clone("Gpu_SetTexWindow", 0x5A7840, 0x13, {}), F(&Gpu_SetTexWindow), 3000);
    // The tail jmp at +6 stays aimed at the C runtime's _ftol.
    FuzzSqrt(Clone("Gte_SquareRoot0", 0x5A7A90, 0xB, {{0x6, nullptr, 0x5B9550}}));
    FuzzApply(Clone("Gte_ApplyMatrixSV", 0x5A7C70, 0x7E, {}));
    bof3::Log("shadow      psx_rest self-test: six leaves, %u MISMATCHES; SquareRoot0 under 0x007F differed in %u of "
              "32768 rounds (expected: not 0)",
              g_leaf_bad, g_sqrt_differ_007f);
    if (g_sqrt_differ_007f == 0)
        bof3::Fatal("psx_rest: SquareRoot0 never differed under a 24-bit control word - the fuzz cannot see precision");

    // --- the teardown chain ---
    BuildFakes();
    CloneAll();
    static State saved, input, theirs, ours;
    Capture(saved);
    const Callees g_saved = g;
    constexpr unsigned kPerKind = 2000;
    constexpr unsigned kRounds = kPerKind * kKinds;
    unsigned bad = 0, bad_per[kKinds] = {}, coverage[2] = {};
    U calls_seen = 0;
    for (unsigned round = 0; round < kRounds; ++round) {
        const unsigned k = round % kKinds;
        g_seed = round * 0x9E3779B9u + 0x7F4A7C15u;
        const Args x = Generate(k, coverage);
        Capture(input);
        for (int pass = 0; pass < 2; ++pass) {
            Apply(input);
            g = k == kTeardownTree ? kTree : kStubs;
            Run(pass ? Ours(k) : g_clones[k], k, x);
            Capture(pass ? ours : theirs);
        }
        calls_seen += theirs.log_n;
        if (std::memcmp(&theirs, &ours, sizeof theirs) != 0) {
            ++bad_per[k];
            if (++bad <= 12)
                bof3::Log("shadow      psx_rest MISMATCH: round %u, %s, log %u / %u (hash %08X / %08X), memory at 0x%X, "
                          "first call %X(%X %X %X) / %X(%X %X %X)",
                          round, kNames[k], (unsigned)theirs.log_n, (unsigned)ours.log_n, (unsigned)theirs.log_hash,
                          (unsigned)ours.log_hash, (unsigned)FirstDifference(theirs, ours), (unsigned)theirs.first[0],
                          (unsigned)theirs.first[1], (unsigned)theirs.first[2], (unsigned)theirs.first[3],
                          (unsigned)ours.first[0], (unsigned)ours.first[1], (unsigned)ours.first[2],
                          (unsigned)ours.first[3]);
        }
    }
    g = g_saved;
    Apply(saved);
    // Each counter counts both passes.
    bof3::Log("shadow      psx_rest self-test: %u rounds (%u per kind, %u kinds), %u MISMATCHES; the originals made %u "
              "calls; fullscreen teardowns %u, error codes 100 on %u, disturbances %u; COM calls (both sides): Release "
              "%u, GetDC %u, RestoreDisplayMode %u, SetCooperativeLevel %u, ReleaseDC %u",
              kRounds, kPerKind, (unsigned)kKinds, bad, (unsigned)calls_seen, coverage[0], coverage[1], g_disturbed,
              g_counts[0x08 / 4], g_counts[0x44 / 4], g_counts[0x4C / 4], g_counts[0x50 / 4], g_counts[0x68 / 4]);
    for (unsigned k = 0; k < kKinds; ++k)
        if (bad_per[k]) bof3::Log("shadow      psx_rest self-test: %s %u of %u rounds differ", kNames[k], bad_per[k], kPerKind);
    if (bad || g_leaf_bad)
        bof3::Fatal("the PSX library layer's last seventeen differ from the originals in %u self-test rounds",
                    bad + g_leaf_bad);
}

}  // namespace psx_rest
