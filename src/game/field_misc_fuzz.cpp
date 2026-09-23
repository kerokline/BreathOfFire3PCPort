// BOF3X_SHADOW=field_misc: a differential fuzz of group M's sixteen functions
// against byte-copies of Capcom's, once at start-up (docs/field-misc.md
// section 3). Every call out of a copy is re-aimed at a recording stand-in -
// the calls among the sixteen included, so each function is tested alone - and
// ours is put on the same stand-ins through field_misc::g. The log is the
// vertex-block harness's (d3d_fuzz.h), whose fake device also takes the three
// draw handlers' COM calls. Per round: the state a function reads, random with
// its boundaries seeded; Capcom's copy, then ours from the same state; the
// calls out with their arguments, the result and every region either could
// write compared.
//
// The stand-ins write what the real callee writes where the caller reads it
// again (Gfx_CommitPrim advances Gfx_PacketNext; MapView_CheckHeightScale sets
// the height scale AreaMap_Slope is handed; AreaMap_Slope its scratch flag), and
// now and then change something the caller reads after the call - a record's
// head or texture dwords, the entry's length, Gfx_BufferIndex, a draw item's
// face word, the primitive, the vertex block, Gfx_DrawTpage - so a value read on
// the wrong side of a call shows.
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <initializer_list>

#include "bof3/symbols.gen.h"
#include "game/d3d_fuzz.h"
#include "game/field_misc_callees.h"
#include "game/move_script_bytes.h"
#include "hook/detour.h"
#include "hook/log.h"

namespace field_misc {
namespace {

using d3d_fuzz::Next;
using d3d_fuzz::Record;

unsigned char* At(U address) { return reinterpret_cast<unsigned char*>(static_cast<std::uintptr_t>(address)); }
U Addr(const void* p) { return static_cast<U>(reinterpret_cast<std::uintptr_t>(p)); }
U GetWord(const unsigned char* p) {
    std::uint16_t v;
    std::memcpy(&v, p, sizeof v);
    return v;
}
U GetLong(const unsigned char* p) {
    U v;
    std::memcpy(&v, p, sizeof v);
    return v;
}
void PutWord(unsigned char* p, U v) {
    const auto w = static_cast<std::uint16_t>(v);
    std::memcpy(p, &w, sizeof w);
}
void PutLong(unsigned char* p, U v) { std::memcpy(p, &v, sizeof v); }
template <typename T, typename F> T As(F f) { return reinterpret_cast<T>(reinterpret_cast<void*>(f)); }
U Pick(std::initializer_list<U> seeds) { return seeds.begin()[Next() % seeds.size()]; }
bool OneIn(U n) { return Next() % n == 0; }

// --- addresses -------------------------------------------------------------------
const U kHeader = at::AreaMap_HeaderAt();             // 0x8CB580, the loaded area block
constexpr U kHeaderBytes = 0x6000;
const U kCells = at::MapView_CellsAt();               // 0x904F20, 0x38 x 0x1C words
constexpr U kCellsBytes = 0x38 * 0x1C * 2;
const U kCellItems = at::MapView_CellItemsAt();       // 0x937FA0, 0x38 x 0x1C x 4 bytes
constexpr U kCellItemsBytes = 0x38 * 0x1C * 4;
const U kDrawItems = at::DrawItemsAt();               // 0x905E80, 0x90 per item
constexpr U kFuzzItems = 0x40;                            // items the fuzz fills and names
constexpr U kDrawItemsBytes = kFuzzItems * 0x90;
const U kClutSource = at::Gfx_ClutStripSourceAt();    // 0x80B580
const U kClutStrip = at::Gfx_ClutStripAt();           // 0x80F580
constexpr U kClutBytes = 0x2000;
const U kScratch = bof3::addr::DamageScratch;             // 0x903850, AreaMap_Slope's sloped flag
const U kHeightScale = at::MapView_HeightScaleAt();   // 0x929F22
const U kViewHeads = at::MapView_ColumnAt();          // 0x929F20: Column, HeightScale, Row
const U kOrigin = at::MapView_OriginAt();             // 0x7E0688, x and z words
const U kPacketNext = at::Gfx_PacketNextAt();         // 0x7E0670
const U kBufferIndex = at::Gfx_BufferIndexAt();       // 0x905B89
const U kOtSlot = at::Draw_OtSlotAt();                // 0x92BF19
const U kRedraw = at::MapView_RedrawAt();             // 0x905E69
const U kBytesPointer = at::AreaMap_BytesAt();        // 0x905D94
const U kDirty = at::Gfx_ClutStripDirtyAt();          // 0x937F90
const U kCellBase = at::AreaMap_CellBaseAt();         // 0x8CB5A4, inside the header region
const U kPatchBase = at::AreaMap_PatchBaseAt();       // 0x8CB5A8

// Buffers of the fuzz's own: the packet pool MapCell_FlatOverlay writes into, a
// primitive for the handlers and setters.
unsigned char g_packets[0x200];
unsigned char g_prim[0x60];

// --- regions ---------------------------------------------------------------------
struct Region {
    U at, size;
};
constexpr U kMaxState = 0x12000;
struct State {
    unsigned char bytes[kMaxState];
};

U RegionBytes(const Region* r, int n) {
    U total = 0;
    for (int i = 0; i < n; ++i) total += r[i].size;
    return total;
}
void Capture(const Region* r, int n, State& s) {
    U at = 0;
    for (int i = 0; i < n; ++i) {
        std::memcpy(s.bytes + at, At(r[i].at), r[i].size);
        at += r[i].size;
    }
}
void Restore(const Region* r, int n, const State& s) {
    U at = 0;
    for (int i = 0; i < n; ++i) {
        std::memcpy(At(r[i].at), s.bytes + at, r[i].size);
        at += r[i].size;
    }
}
bool FirstDifference(const Region* r, int n, const State& a, const State& b, U* where) {
    U at = 0;
    for (int i = 0; i < n; ++i) {
        for (U k = 0; k < r[i].size; ++k)
            if (a.bytes[at + k] != b.bytes[at + k]) {
                *where = r[i].at + k;
                return true;
            }
        at += r[i].size;
    }
    return false;
}

// Every region any test touches, saved once and put back at the end.
Region g_all[] = {
    {kHeader, kHeaderBytes}, {kCells - 4, kCellsBytes + 4}, {kCellItems, kCellItemsBytes},
    {kDrawItems, kDrawItemsBytes}, {kClutSource, kClutBytes}, {kClutStrip, kClutBytes},
    {kOrigin, 4},            {kViewHeads, 6},             {kScratch, 1},
    {kRedraw, 1},            {kBytesPointer, 4},          {kBufferIndex, 1},
    {kPacketNext, 4},        {kOtSlot, 1},                {kDirty, 1},
    {kVertices, 0x80},       {kScaleY, 8},                {kDrawTpage, 4},
};

// --- the stand-ins -----------------------------------------------------------------

U g_round;
U Mix(U salt) {
    U h = (g_round * 0x9E3779B1u) ^ (salt * 0x85EBCA6Bu) ^ (d3d_fuzz::g_log->n * 0xC2B2AE35u);
    h ^= h >> 16;
    h *= 0x7FEB352Du;
    h ^= h >> 15;
    h *= 0x846CA68Bu;
    h ^= h >> 16;
    return h;
}

// A byte answer with stale upper bits (the callers read al): mostly 0, then 1,
// 0x7F, 0x80, 0xFF; one round in eight every non-zero answer 0x80 (a byte sum
// that wraps).
U ByteAnswer(U salt) {
    const U h = Mix(salt);
    U al;
    if (g_round % 8 == 0) al = h % 2 ? 0x80 : 0;
    else {
        const U k = (h >> 3) % 12;
        al = k < 7 ? 0 : k < 9 ? 1 : k == 9 ? 0x7F : k == 10 ? 0x80 : 0xFF;
    }
    return (h & 0xFFFFFF00u) | al;
}

// The current test's context, for the disturbances.
unsigned char* g_entry;        // AreaMap_ApplyPatch's entry
unsigned g_item_hint;          // an item whose face words may be disturbed
unsigned char* g_prim_now;     // the handler's primitive
U g_prim_bytes;

U __cdecl StubCell(U x, U y) {
    Record(1, x & 0xFFFF, y & 0xFFFF);
    return ByteAnswer(1);
}
U __cdecl StubSteep(U x, U y) {
    Record(2, x, y);
    return ByteAnswer(2);
}
U __cdecl StubSteepAt(U x, U y) {
    Record(3, x, y);
    return ByteAnswer(3);
}
// Gpu_SetPolyF4 writes the code and the four z; then, now and then, moves
// Gfx_PacketNext (read once, before), Gfx_BufferIndex or a face word (read
// after), or the colour words.
unsigned char* __cdecl StubSetPolyF4(unsigned char* p) {
    Record(4, Addr(p));
    p[7] = 0x28;
    for (U z = 0x10; z <= 0x34; z += 0xC) PutLong(p + z, 0x3C23D70A);
    const U h = Mix(4);
    switch ((h >> 4) % 8) {
    case 0: PutLong(At(kPacketNext), GetLong(At(kPacketNext)) + 0x38); break;
    case 1: At(kBufferIndex)[0] ^= 1; break;
    case 2: PutWord(At(kItemFaceA + g_item_hint * 0x90), (h >> 8) % kFuzzItems); break;
    case 3: PutWord(At(kItemFaceB + g_item_hint * 0x90), (h >> 8) % kFuzzItems); break;
    case 4: PutWord(At(kOverlayColours), h >> 12); break;
    default: break;
    }
    return reinterpret_cast<unsigned char*>(static_cast<std::uintptr_t>(h & 0xFF));   // eax unread
}
// Gfx_CommitPrim advances the pool when there is room, which is always here.
void __cdecl StubCommit(U slot, U size) {
    Record(5, slot & 0xFF, size & 0xFF);
    PutLong(At(kPacketNext), GetLong(At(kPacketNext)) + (size & 0xFF));
    const U h = Mix(5);
    if (h % 4 == 0) At(kOtSlot)[0] = static_cast<unsigned char>(h >> 8);
    if (h % 5 == 0) At(kBufferIndex)[0] ^= 1;
}
// Area_TestCondition reads the low word. The entry's length is read after it.
U __cdecl StubCondition(U code) {
    Record(6, code & 0xFFFF);
    const U h = Mix(6);
    if (g_entry && h % 6 == 0) PutWord(g_entry + 2, (h >> 8) % 24);
    const U k = (h >> 12) % 16;
    const U al = k < 7 ? 0 : k < 14 ? 1 : k == 14 ? 2 : 0xFF;
    return (h & 0xFFFFFF00u) | al;
}
// MapView_ItemAt: 0 a third of the time, else an item the fuzz filled. Now
// and then it changes one of the entry's first ten dwords - a record's face
// bits (read before the call) or a whole dword (a head, or the texture dword
// read after it) - or Gfx_BufferIndex (read after).
U __cdecl StubItemAt(U x, U y) {
    Record(7, x, y);
    const U h = Mix(7);
    if (g_entry) {
        unsigned char* const r = g_entry + 4 + ((h >> 24) % 10) * 4;
        switch ((h >> 20) % 6) {
        case 0: r[2] ^= static_cast<unsigned char>((h & 0xF0) | 0x10); break;
        case 1: PutLong(r, Mix(70)); break;
        case 2: At(kBufferIndex)[0] ^= 1; break;
        default: break;
        }
    }
    if (h % 3 == 0) return 0;
    const U item = (h >> 4) % kFuzzItems;
    g_item_hint = item;
    return item ? item : 1;
}
void __cdecl StubSetTexture(U texture, unsigned char* half, U count) { Record(8, texture, Addr(half), count); }
void __cdecl StubCheckScale() {
    Record(9);
    const U h = Mix(9);
    if (h % 2) At(kHeightScale)[0] = static_cast<unsigned char>(h >> 8);
}
U __cdecl StubSlope(U x, U y, U direction) {
    Record(10, x, y, direction, At(kHeightScale)[0]);
    const U h = Mix(10);
    At(kScratch)[0] = static_cast<unsigned char>(h >> 24);
    return Mix(11);
}

// The draw handlers' helpers, as d3d_draw_fuzz.cpp's: the colour written, and a
// quarter of the time one byte of what the handlers read changed.
void Disturb(U salt) {
    const U h = Mix(salt ^ 0x5BD1E995u);
    if (h % 4) return;
    const auto value = static_cast<unsigned char>(h >> 24);
    switch ((h >> 2) % 5) {
    case 0:
    case 1:
        if (g_prim_now) g_prim_now[(h >> 8) % g_prim_bytes] = value;
        break;
    case 2: At(kVertices)[(h >> 8) % 0x80] = value; break;
    case 3: At(kScaleY)[(h >> 8) % 8] = value; break;
    default: At(kDrawTpage)[(h >> 8) % 2] = value; break;
    }
}
void __cdecl StubPrimColor(U r, U gg, U b, U code, U mode, unsigned long* diffuse, unsigned long* specular) {
    Record(11, r, gg, b, code, mode, specular != nullptr);
    Disturb(12);
    *diffuse = Mix(13);
    if (specular) *specular = Mix(14);
}
void __cdecl StubRet(U a) {
    Record(12, a);
    Disturb(15);
}
void __cdecl StubBlend(U code, U mode) {
    Record(13, code, mode);
    Disturb(16);
}
void __cdecl StubShade(U mode) {
    Record(14, mode);
    Disturb(17);
}

const Callees kStandIns = {
    As<unsigned char (__cdecl*)(short, short)>(&StubCell),
    As<unsigned char (__cdecl*)(long, long)>(&StubSteep),
    As<unsigned char (__cdecl*)(long, long)>(&StubSteepAt),
    StubSetPolyF4,
    As<void (__cdecl*)(unsigned, unsigned)>(&StubCommit),
    As<unsigned char (__cdecl*)(unsigned long)>(&StubCondition),
    As<unsigned long (__cdecl*)(long, long)>(&StubItemAt),
    As<void (__cdecl*)(unsigned long, unsigned char*, int)>(&StubSetTexture),
    StubCheckScale,
    As<long (__cdecl*)(long, long, unsigned long)>(&StubSlope),
    As<void (__cdecl*)(unsigned, unsigned, unsigned, unsigned, unsigned, unsigned long*, unsigned long*)>(
        &StubPrimColor),
    As<void (__cdecl*)(unsigned)>(&StubRet),
    As<void (__cdecl*)(unsigned, unsigned)>(&StubBlend),
    As<void (__cdecl*)(unsigned)>(&StubShade),
};

// --- the copies --------------------------------------------------------------------
// Every E8 of each body, by capstone 2026-09-23 (docs/field-misc.md section 1);
// every jump stays inside. AreaMap_BlockedNarrow's jump table is moved into its
// copy.

const void* StubFor(U target) {
    switch (target) {
    case 0x518620: return As<const void*>(&StubCell);
    case 0x518760: return As<const void*>(&StubSteep);
    case 0x5187A0: return As<const void*>(&StubSteepAt);
    case 0x5A75B0: return As<const void*>(&StubSetPolyF4);
    case 0x461E50: return As<const void*>(&StubCommit);
    case 0x56FF00: return As<const void*>(&StubCondition);
    case 0x572ED0: return As<const void*>(&StubItemAt);
    case 0x572A00: return As<const void*>(&StubSetTexture);
    case 0x572590: return As<const void*>(&StubCheckScale);
    case 0x5722D0: return As<const void*>(&StubSlope);
    case 0x59FBA0: return As<const void*>(&StubPrimColor);
    case 0x437CC0: return As<const void*>(&StubRet);
    case 0x59FCA0: return As<const void*>(&StubBlend);
    case 0x59FD80: return As<const void*>(&StubShade);
    default: bof3::Fatal("field_misc: no stand-in for a call to 0x%X", (unsigned)target);
    }
}

struct Site {
    U offset, target;
};
constexpr Site kNarrowSites[] = {
    {0x28, 0x518620},  {0x4A, 0x518620},  {0x5A, 0x5187A0},  {0x78, 0x518620},  {0x9A, 0x518620},
    {0xAB, 0x5187A0},  {0xBA, 0x5187A0},  {0xE1, 0x518620},  {0xFF, 0x518620},  {0x10F, 0x5187A0},
    {0x11E, 0x5187A0}, {0x139, 0x518620}, {0x153, 0x518620}, {0x164, 0x5187A0}, {0x173, 0x5187A0},
    {0x19A, 0x518620}, {0x1B4, 0x518620}, {0x1C4, 0x5187A0}, {0x1DE, 0x518620}, {0x1F8, 0x518620},
    {0x209, 0x5187A0}, {0x218, 0x5187A0}, {0x224, 0x518760},
};
constexpr Site kSteepAtSites[] = {{0xA, 0x518760}};
constexpr Site kOverlaySites[] = {{0x20, 0x5A75B0}, {0xF8, 0x461E50}};
constexpr Site kPatchSites[] = {{0xE, 0x56FF00}, {0x139, 0x572ED0}, {0x1A6, 0x572A00}};
constexpr Site kSlopeSites[] = {{0x0, 0x572590}, {0x14, 0x5722D0}};
constexpr Site kPolyF4Sites[] = {{0x31, 0x59FBA0}, {0x13C, 0x437CC0}, {0x143, 0x437CC0}, {0x159, 0x59FCA0},
                                 {0x160, 0x59FD80}};
constexpr Site kPolyG4Sites[] = {{0x34, 0x59FBA0},  {0x65, 0x59FBA0},  {0x96, 0x59FBA0},  {0xCA, 0x59FBA0},
                                 {0x1E3, 0x437CC0}, {0x1EA, 0x437CC0}, {0x200, 0x59FCA0}, {0x207, 0x59FD80}};
constexpr Site kLineF3Sites[] = {{0x31, 0x59FBA0}, {0x101, 0x437CC0}, {0x108, 0x437CC0}, {0x120, 0x59FCA0},
                                 {0x127, 0x59FD80}};

void* Clone(const char* name, U base, U size, const Site* sites, int n) {
    bof3::CloneCall calls[24];
    if (n > 24) bof3::Fatal("field_misc: %s has %d calls", name, n);
    for (int i = 0; i < n; ++i) calls[i] = {sites[i].offset, StubFor(sites[i].target), sites[i].target};
    void* code = bof3::CloneOriginal(name, base, size, calls, n);
    if (!code) bof3::Fatal("field_misc: CloneOriginal(%s) returned null", name);
    return code;
}

// --- the comparison ------------------------------------------------------------------

d3d_fuzz::Log g_theirs, g_ours;
State g_start, g_after_theirs, g_after_ours;

struct Tally {
    const char* name;
    unsigned rounds, bad, calls, hits;   // hits: a coverage count each test defines
};

// One round: state already generated. Runs Capcom's copy then ours from the same
// state and compares the log, (result & mask) and the regions.
template <typename Theirs, typename Ours>
void Pass(Tally& t, const Region* rs, int nr, U mask, Theirs theirs, Ours ours) {
    Capture(rs, nr, g_start);
    g_theirs.Clear();
    d3d_fuzz::g_log = &g_theirs;
    const U ret_theirs = theirs();
    Capture(rs, nr, g_after_theirs);

    Restore(rs, nr, g_start);
    g_ours.Clear();
    d3d_fuzz::g_log = &g_ours;
    const U ret_ours = ours();
    Capture(rs, nr, g_after_ours);
    d3d_fuzz::g_log = nullptr;

    ++t.rounds;
    t.calls += g_theirs.n;
    char why[200] = "";
    bool same = d3d_fuzz::SameLog(g_ours, g_theirs, why);
    if (same && (ret_ours & mask) != (ret_theirs & mask)) {
        std::snprintf(why, sizeof why, "the result %08X, the original %08X", (unsigned)ret_ours,
                      (unsigned)ret_theirs);
        same = false;
    }
    U where = 0;
    if (same && FirstDifference(rs, nr, g_after_ours, g_after_theirs, &where)) {
        std::snprintf(why, sizeof why, "memory at 0x%X", (unsigned)where);
        same = false;
    }
    if (!same) {
        if (t.bad < 4) bof3::Log("shadow      field_misc MISMATCH: %s round %u: %s", t.name, t.rounds - 1, why);
        ++t.bad;
    }
}

void FillRandom(U at, U bytes) {
    for (U i = 0; i < bytes; i += 4) PutLong(At(at + i), Next());
}

// --- AreaMap_BlockedNarrow, AreaMap_TooSteepAt ------------------------------------------

U Fraction() { return Next() % 3 ? Pick({0, 0x8000, 0xFFFF, 1, 0x7FFF, 0}) : Next() & 0xFFFF; }
U Point() {
    const U cell = Next() % 3 ? Next() % 0x40 : Pick({0, 0xFFFF, 0x7FFF, 0x8000, 0xFFFE, Next() & 0xFFFF});
    return (cell << 16) | Fraction();
}
U Direction() {
    U d;
    switch (Next() % 4) {
    case 0:
    case 1: d = Pick({1, 3, 5, 7}); break;
    case 2: d = Pick({0, 2, 4, 6, 8, 0xFF, 9}); break;
    default: d = Next() & 0xFF; break;
    }
    return Next() % 2 ? d : (Next() & 0xFFFFFF00u) | d;
}

void FuzzNarrow(Tally& t, void* clone, unsigned rounds) {
    using Fn = U(__cdecl*)(U, U, U);
    for (unsigned r = 0; r < rounds; ++r) {
        g_round = 0x100000u + r;
        const U x = Point(), y = Point(), d = Direction();
        Pass(t, nullptr, 0, 0xFF, [&] { return reinterpret_cast<Fn>(clone)(x, y, d); },
             [&] { return As<Fn>(&AreaMap_BlockedNarrow)(x, y, d); });
        if ((d & 0xFF) == 1 || (d & 0xFF) == 3 || (d & 0xFF) == 5 || (d & 0xFF) == 7) ++t.hits;
    }
}

void FuzzSteepAt(Tally& t, void* clone, unsigned rounds) {
    using Fn = U(__cdecl*)(U, U);
    for (unsigned r = 0; r < rounds; ++r) {
        g_round = 0x200000u + r;
        const U x = Next(), y = Next();
        Pass(t, nullptr, 0, 0xFF, [&] { return reinterpret_cast<Fn>(clone)(x, y); },
             [&] { return As<Fn>(&AreaMap_TooSteepAt)(x, y); });
    }
}

// --- MapView_SlopeAt ------------------------------------------------------------------------

const Region kSlopeRegions[] = {{kHeightScale, 1}, {kScratch, 1}};

void FuzzSlopeAt(Tally& t, void* clone, unsigned rounds) {
    using Fn = U(__cdecl*)(U, U, U);
    for (unsigned r = 0; r < rounds; ++r) {
        g_round = 0x300000u + r;
        const U x = Next(), y = Next(), d = Next() % 2 ? Next() % 8 : Next();
        At(kHeightScale)[0] = static_cast<unsigned char>(Next() % 3 ? Next() % 2 : Next());
        At(kScratch)[0] = static_cast<unsigned char>(Next());
        Pass(t, kSlopeRegions, 2, 0xFFFFFFFFu, [&] { return reinterpret_cast<Fn>(clone)(x, y, d); },
             [&] { return As<Fn>(&MapView_SlopeAt)(x, y, d); });
    }
}

// --- the view ring: MapView_ItemAt, MoveCmd_TestFB / FC -------------------------------------

// The ring heads: in range mostly (the grid's own), -1 now and then; `wide`
// also past the end (the one-subtraction wrap then leaves the grid: the reads
// land after it, inside .data - MapView_ItemAt only).
void RandomView(bool wide) {
    PutWord(At(kOrigin), Next() % 2 ? Next() : Pick({0, 0xFFFF, 0x8000, 0x7FFF, 0xFFF0}));
    PutWord(At(kOrigin + 2), Next() % 2 ? Next() : Pick({0, 0xFFFF, 0x8000, 0x7FFF, 0xFFF0}));
    U row = Next() % 5 ? Next() % 0x38 : Pick({0, 0x37, 0xFFFF, 0x1B});
    U column = Next() % 5 ? Next() % 0x1C : Pick({0, 0x1B, 0xFFFF, 0xD, 0xFFFE, 0xFFFE, 0xFFFE});
    if (wide && OneIn(6)) row = 0x38 + Next() % 0x10;
    if (wide && OneIn(6)) column = 0x1C + Next() % 0x10;
    PutWord(At(at::MapView_RowAt()), row);
    PutWord(At(at::MapView_ColumnAt()), column);
}
// A map (dx, dz) relative to the origin: on the ring's diamond mostly, its
// edges seeded (sum and difference 0, 0x37, 0x38, -1).
void RingPoint(std::int32_t* dx, std::int32_t* dz) {
    switch (Next() % 4) {
    case 0: {
        const std::int32_t s = static_cast<std::int32_t>(Pick({0, 0x37, 0x38, 0, 0x37})) - (OneIn(4) ? 1 : 0);
        const std::int32_t d = static_cast<std::int32_t>(Pick({0, 0x37, 0x38, 1, 0x36})) - (OneIn(4) ? 1 : 0);
        *dx = (s + d) / 2;
        *dz = s - *dx;
        break;
    }
    case 1:
        *dx = static_cast<std::int32_t>(Next() % 0x60) - 0x10;
        *dz = static_cast<std::int32_t>(Next() % 0x60) - 0x30;
        break;
    default: {
        const std::int32_t s = static_cast<std::int32_t>(Next() % 0x38), d = static_cast<std::int32_t>(Next() % 0x38);
        *dx = (s + d) / 2;
        *dz = *dx - d;
        if (((s + d) & 1) && OneIn(2)) ++*dx;
        break;
    }
    }
}

void FuzzItemAt(Tally& t, void* clone, unsigned rounds) {
    using Fn = U(__cdecl*)(U, U);
    for (unsigned r = 0; r < rounds; ++r) {
        g_round = 0x400000u + r;
        RandomView(true);
        if (r % 16 == 0) FillRandom(kCellItems, kCellItemsBytes);
        std::int32_t dx, dz;
        RingPoint(&dx, &dz);
        const auto ox = static_cast<U>(static_cast<std::int16_t>(GetWord(At(kOrigin))));
        const auto oz = static_cast<U>(static_cast<std::int16_t>(GetWord(At(kOrigin + 2))));
        U x = ox + static_cast<U>(dx), y = oz + static_cast<U>(dz);
        if (OneIn(8)) x ^= 0x10000u << (Next() % 16);   // the whole dword counts here
        if (OneIn(16)) y = Next();
        U ret = 0;
        Pass(t, nullptr, 0, 0xFFFFFFFFu,
             [&] {
                 ret = reinterpret_cast<Fn>(clone)(x, y);
                 return ret;
             },
             [&] { return As<Fn>(&MapView_ItemAt)(x, y); });
        if (ret) ++t.hits;
    }
}

// The runs a cell word names, laid out in the header region: each at a dword
// index from 0x40, its count in the dword before, its records stepping by byte
// +2 to exactly the last dword (steps of 1..3 - a 0 or an overshoot runs on in
// the original and in ours alike, into memory the fuzz does not own).
constexpr unsigned kRuns = 8;
U g_run_index[kRuns];

void BuildRuns() {
    U next = 0x40 + Next() % 8;
    for (unsigned k = 0; k < kRuns; ++k) {
        const U count = Next() % 4 ? 1 + Next() % 12 : Pick({1, 2, 16});
        g_run_index[k] = next;
        const U head = kHeader + (next - 1) * 4;
        PutLong(At(head), (count << 16) | (Next() & 0xFFFF));
        // records at 0 .. count - 2, the last dword (count - 1) a terminator
        U pos = 0;
        while (pos + 1 < count) {
            const U left = count - 1 - pos;
            const U step = 1 + Next() % (left < 3 ? left : 3);
            U kind = Next() % 2 ? 0x23 : Pick({0x24, 0x22, 0x63, 0xA3, 0x00, Next() & 0xFF});
            const U low = Next() % 3 ? Next() % 0x300 : Pick({0, 1, 0x2FF});
            PutLong(At(kHeader + (next + pos) * 4), (kind << 24) | (step << 16) | (low & 0xFFFF));
            for (U k2 = 1; k2 < step; ++k2) PutLong(At(kHeader + (next + pos + k2) * 4), Next());
            pos += step;
        }
        PutLong(At(kHeader + (next + count - 1) * 4), Next());
        next += count + 1 + Next() % 4;
    }
}

void FuzzTileTest(Tally& t, void* clone, U fn_ours, unsigned rounds, U salt) {
    using Fn = U(__cdecl*)(U, U);
    const Region regions[] = {{kHeader, 0x1400}, {kCells - 4, kCellsBytes + 4}, {kOrigin, 4}, {kViewHeads, 6}};
    for (unsigned r = 0; r < rounds; ++r) {
        g_round = salt + r;
        RandomView(false);
        // The header region: the patch entries random under seeded masks.
        for (U i = 0; i < 0x1400; i += 4) {
            U v = Next();
            if (Next() % 4) v = (v & ~0xF001u) | Pick({0x8000, 0x8001, 0x8000, 0x8001, 0x0000, 0x0001, 0x9000, 0xA001, 0x4000, 0xF001});
            PutLong(At(kHeader + i), v);
        }
        // AreaMap_CellBase and AreaMap_PatchBase: the low halves count.
        const U cell_base = Next() % 0x20, patch_base = Next() % 0x100;
        PutLong(At(kCellBase), (Next() & 0xFFFF0000u) | cell_base);
        PutLong(At(kPatchBase), (Next() & 0xFFFF0000u) | patch_base);
        BuildRuns();
        for (U i = 0; i < kCellsBytes; i += 2)
            PutWord(At(kCells + i), Next() % 5 < 3 ? 0 : g_run_index[Next() % kRuns] - cell_base);
        // A column head of -2 can put ring row 0's first cell one word before the
        // grid: that word is the fuzz's, and empty.
        PutLong(At(kCells - 4), 0);
        std::int32_t dx, dz;
        RingPoint(&dx, &dz);
        // Aim the cell hit: the ring slot of (dx, dz), when on the grid, gets a run.
        const std::int32_t s = dx + dz, d = dx - dz;
        if (s >= 0 && s < 0x38 && d >= 0 && d < 0x38 && Next() % 4) {
            std::int32_t row = s + static_cast<std::int16_t>(GetWord(At(at::MapView_RowAt()))) + 1;
            if (row >= 0x38) row -= 0x38;
            std::int32_t col = d / 2 + static_cast<std::int16_t>(GetWord(At(at::MapView_ColumnAt()))) + 1;
            if (col >= 0x1C) col -= 0x1C;
            PutWord(At(kCells + static_cast<U>(col + row * 28) * 2), g_run_index[Next() % kRuns] - cell_base);
        }
        // The words, less the origin, with stale upper halves (the callee reads 16 bits).
        const U x = ((GetWord(At(kOrigin)) + static_cast<U>(dx)) & 0xFFFF) | (Next() % 2 ? Next() << 16 : 0);
        const U z = ((GetWord(At(kOrigin + 2)) + static_cast<U>(dz)) & 0xFFFF) | (Next() % 2 ? Next() << 16 : 0);
        U flag = 0;
        Pass(t, regions, 4, 0xFFFFFFFFu,
             [&] {
                 const U ret = reinterpret_cast<Fn>(clone)(x, z);
                 flag = ret == 1;
                 return ret;
             },
             [&] { return reinterpret_cast<Fn>(static_cast<std::uintptr_t>(fn_ours))(x, z); });
        t.hits += flag;
    }
}

// --- AreaMap_ApplyPatch -------------------------------------------------------------------------

constexpr U kEntryAt = 0x1000;   // the entry, inside the header region, where the stores can reach it
constexpr U kBytesAt = 0x4800;   // where AreaMap_Bytes points, inside it too

void RandomDrawItems() {
    for (U i = 0; i < kDrawItemsBytes; i += 4) PutLong(At(kDrawItems + i), Next());
    for (U item = 0; item < kFuzzItems; ++item) {
        PutWord(At(kItemFaceA + item * 0x90), Next() % kFuzzItems);
        PutWord(At(kItemFaceB + item * 0x90), Next() % kFuzzItems);
    }
}

void FuzzApplyPatch(Tally& t, void* clone, unsigned rounds) {
    using Fn = void(__cdecl*)(const unsigned char*);
    const Region regions[] = {{kHeader, kHeaderBytes}, {kRedraw, 1}, {kBytesPointer, 4}, {kBufferIndex, 1}};
    for (unsigned r = 0; r < rounds; ++r) {
        g_round = 0x500000u + r;
        if (r % 64 == 0) RandomDrawItems();
        // The header region as small words: every texture-run tile word read stays
        // small, so every store stays inside the region.
        for (U i = 0; i < kHeaderBytes; i += 2) PutWord(At(kHeader + i), Next() & 0x7FF);
        const U width = Next() % 4 ? 1 + Next() % 16 : Pick({0, 1, 16});
        const U height = Next() % 4 ? 1 + Next() % 16 : Pick({0, 1, 16});
        At(kHeader)[0] = static_cast<unsigned char>(width);
        At(kHeader)[1] = static_cast<unsigned char>(height);
        PutWord(At(kHeader + 2), 0x10 + Next() % 0xF0);   // the offset: at least 0x10, so no store reaches the header's first dword
        PutLong(At(kBytesPointer), kHeader + kBytesAt);
        At(kBufferIndex)[0] = static_cast<unsigned char>(Next() % 2);
        At(kRedraw)[0] = static_cast<unsigned char>(Next() % 4);
        // The entry: a condition code, a length n, then records of kinds 0, 1, 2,
        // others, whatever n says - the last record may run past n.
        unsigned char* const entry = At(kHeader + kEntryAt);
        const U n = Next() % 8 ? 1 + Next() % 24 : Pick({0, 1, 2, 3});
        PutLong(entry, (n << 16) | (Next() & 0xFFFF));
        for (U k = 1; k < 0x28; ++k) {
            U v = Next();
            const U kind = Pick({0, 0, 1, 1, 2, 2, 3, 0x80, 0xFF, 0x24});
            const U face = Pick({0, 1, 2, 3, 0xF});
            const U x = Next() % 3 ? Next() % 16 : Next() % 256;
            const U y = Next() % 3 ? Next() % 16 : Next() % 256;
            if (Next() % 3) v = (kind << 24) | (face << 20) | ((Next() % 16) << 16) | (x << 8) | y;
            PutLong(entry + k * 4, v);
        }
        g_entry = entry;
        Pass(t, regions, 4, 0, [&] { reinterpret_cast<Fn>(clone)(entry); return 0u; },
             [&] { AreaMap_ApplyPatch(entry); return 0u; });
        for (unsigned c = 0; c < g_theirs.n && c < d3d_fuzz::kMaxCalls; ++c)
            if (g_theirs.calls[c].what == 8) {
                ++t.hits;
                break;
            }
        g_entry = nullptr;
    }
}

// --- MapCell_FlatOverlay ----------------------------------------------------------------------

void FuzzOverlay(Tally& t, void* clone, unsigned rounds) {
    using Fn = void(__cdecl*)(U, U);
    const Region regions[] = {{Addr(g_packets), sizeof g_packets}, {kPacketNext, 4},  {kBufferIndex, 1},
                              {kOtSlot, 1},                        {kDrawItems, kDrawItemsBytes},
                              {kHeader + 0x10, 0x30}};
    for (unsigned r = 0; r < rounds; ++r) {
        g_round = 0x600000u + r;
        RandomDrawItems();
        for (U i = 0; i < sizeof g_packets; ++i) g_packets[i] = static_cast<unsigned char>(Next());
        FillRandom(kHeader + 0x10, 0x30);
        PutLong(At(kPacketNext), Addr(g_packets) + (Next() % 2 ? 0 : 4));
        At(kBufferIndex)[0] = static_cast<unsigned char>(Next() % 2);
        At(kOtSlot)[0] = static_cast<unsigned char>(Next());
        // faces: kind - 0x10 of the handler's kinds, seeded; now and then any byte less 0x10
        U faces = Next() % 3 ? Next() % 16 : Pick({0, 1, 2, 4, 7, 8, 9, 0xF});
        if (OneIn(8)) faces = (Next() & 0xFF) - 0x10;
        const U item = Next() % kFuzzItems;
        g_item_hint = item;
        Pass(t, regions, 6, 0, [&] { reinterpret_cast<Fn>(clone)(faces, item); return 0u; },
             [&] { As<Fn>(&MapCell_FlatOverlay)(faces, item); return 0u; });
        t.hits += faces & 1;
    }
}

// --- Gfx_ClutAdjust ------------------------------------------------------------------------------

void FuzzClut(Tally& t, void* clone, unsigned rounds) {
    using Fn = U(__cdecl*)(U, U, U, U, U);
    const Region regions[] = {{kClutSource, kClutBytes}, {kClutStrip, kClutBytes}, {kDirty, 1}};
    for (unsigned r = 0; r < rounds; ++r) {
        g_round = 0x700000u + r;
        FillRandom(kClutSource, kClutBytes);
        if (OneIn(2))   // channels at 0 and at 0x1F
            for (U i = 0; i < kClutBytes; i += 2)
                PutWord(At(kClutSource + i), GetWord(At(kClutSource + i)) & Pick({0x7C1F, 0x83E0, 0x7FFF, 0x8000, 0xFFFF}));
        FillRandom(kClutStrip, kClutBytes);
        At(kDirty)[0] = static_cast<unsigned char>(Next());
        const U columns = Next() % 3 ? Next() & 0xFFFF : Pick({0, 0xFFFFFFFFu, 0x80000001u, 1, 0xFFFF, Next()});
        const U rows = Next() % 3 ? Next() & 0xFFF : Pick({0, 0xFFFFFFFFu, 0x80000001u, 1, 0xFFF, Next()});
        U add[3];
        for (U& a : add)
            a = Next() % 2 ? static_cast<U>(static_cast<std::int32_t>(Next() % 65) - 32)
                           : Pick({0, 1, 0xFFFFFFFFu, 0x1F, 0xFFFFFFE1u, 0x7FFFFFFFu, 0x80000000u, 0x20});
        Pass(t, regions, 3, 0xFFFFFFFFu, [&] { return reinterpret_cast<Fn>(clone)(columns, rows, add[0], add[1], add[2]); },
             [&] { return As<Fn>(&Gfx_ClutAdjust)(columns, rows, add[0], add[1], add[2]); });
    }
}

// --- the draw handlers ---------------------------------------------------------------------------

unsigned short GetControlWord() {
    unsigned short cw;
    __asm__ volatile("fnstcw %0" : "=m"(cw));
    return cw;
}
void SetControlWord(unsigned short cw) { __asm__ volatile("fldcw %0" : : "m"(cw)); }
const unsigned short kControlWords[] = {0x027F, 0x007F, 0x037F};

const U kFloats[] = {
    0x00000000, 0x80000000, 0x3F800000, 0x3F000000, 0x43200000, 0xC2000000, 0x3C23D70A,  // 0 -0 1 .5 160 -32 .01
    0x7149F2CA, 0x7F7FFFFF, 0x00800000, 0x007FFFFF, 0x00000001, 0x7F800000, 0xFF800000,  // 1e30 max min-normal denormals inf
    0x7FC00000, 0x7FA00000, 0xFF800001, 0x3F7D70A4, 0x4B7FFFFF, 0x3EAAAAAB, 0xBF800000,  // qNaN sNaN sNaN .99 2^24-1 1/3 -1
};
const U kScales[] = {0x40000000, 0x3F800000, 0x3FC00000, 0x40400000, 0x3F000000, 0xC0000000, 0x40100000,
                     0x0DA24260, 0x7F000000, 0x3F800001};

struct Handler {
    const char* name;
    U base, size;
    const Site* sites;
    int n_sites;
    const void* ours;
    U prim_bytes;
    U corners, stride;   // float corners from +8
    bool gouraud;        // colours at +4 + i * 0x10
};

void FuzzHandler(Tally& t, const Handler& h, void* clone, unsigned rounds) {
    using Fn = U(__cdecl*)(unsigned char*);
    const Region regions[] = {{kVertices, 0x80}, {kScaleY, 8}, {kDrawTpage, 4}, {Addr(g_prim), sizeof g_prim}};
    const unsigned short saved = GetControlWord();
    for (unsigned r = 0; r < rounds; ++r) {
        g_round = (h.base << 4) + r;
        for (U a : {kScaleX, kScaleY})
            PutLong(At(a), Next() % 5 == 0 ? Next() : Next() % 5 == 1 ? 0x3F800000u + Next() % 0x2000000u
                                                                         : kScales[Next() % 10]);
        FillRandom(kVertices, 0x80);
        PutLong(At(kDrawTpage), Next() % 2 ? Next() : (Next() & 0xFFFF0000u) | Pick({0, 0x20, 0x40, 0x60, 0xFFFF}));
        for (U i = 0; i < sizeof g_prim; ++i) g_prim[i] = static_cast<unsigned char>(Next());
        for (U c = 0; c < h.corners; ++c)
            for (U f = 0; f < 3; ++f)
                if (Next() % 4) PutLong(g_prim + 8 + c * h.stride + f * 4, Next() % 3 ? kFloats[Next() % 21] : Next());
        for (U c = 0; c < (h.gouraud ? 4u : 1u); ++c)
            for (U i = 0; i < 3; ++i)
                if (Next() % 2) g_prim[4 + c * 0x10 + i] = static_cast<unsigned char>(Pick({0, 1, 0x7F, 0x80, 0xFF}));
        g_prim_now = g_prim;
        g_prim_bytes = h.prim_bytes;
        const unsigned short cw = kControlWords[Next() % 3];
        Pass(t, regions, 4, 0xFFFFFFFFu,
             [&] {
                 SetControlWord(cw);
                 const U ret = reinterpret_cast<Fn>(clone)(g_prim);
                 SetControlWord(saved);
                 return ret;
             },
             [&] {
                 SetControlWord(cw);
                 const U ret = reinterpret_cast<Fn>(const_cast<void*>(h.ours))(g_prim);
                 SetControlWord(saved);
                 return ret;
             });
        g_prim_now = nullptr;
    }
}

// --- the setters ---------------------------------------------------------------------------------

void FuzzSetter(Tally& t, void* clone, const void* ours, bool move, unsigned rounds) {
    const Region regions[] = {{Addr(g_prim), sizeof g_prim}};
    for (unsigned r = 0; r < rounds; ++r) {
        g_round = 0x800000u + r;
        for (U i = 0; i < sizeof g_prim; ++i) g_prim[i] = static_cast<unsigned char>(Next());
        unsigned char* const prim = g_prim + (Next() % 2 ? 0 : 8);
        if (move) {
            // the rect inside the primitive now and then: it is read after +4 is written
            const unsigned char* rect = g_prim + Pick({0x38, 0x40, 0, 4, 8, 0xC});
            const U x = Next(), y = Next();
            using Fn = U(__cdecl*)(unsigned char*, const unsigned char*, U, U);
            Pass(t, regions, 1, 0xFFFFFFFFu, [&] { return reinterpret_cast<Fn>(clone)(prim, rect, x, y); },
                 [&] { return reinterpret_cast<Fn>(const_cast<void*>(ours))(prim, rect, x, y); });
        } else {
            using Fn = U(__cdecl*)(unsigned char*);
            Pass(t, regions, 1, 0xFFFFFFFFu, [&] { return reinterpret_cast<Fn>(clone)(prim); },
                 [&] { return reinterpret_cast<Fn>(const_cast<void*>(ours))(prim); });
        }
    }
}

}  // namespace

void SelfTest() {
    // The copies, before FieldMisc_Inject patches anything.
    void* narrow = Clone("AreaMap_BlockedNarrow", 0x5183C0, 0x25C, kNarrowSites, 23);
    move_script::Relocate(narrow, 0x5183C0, 0x240, {0x1A, 0x240, 7});
    void* steep_at = Clone("AreaMap_TooSteepAt", 0x5187A0, 0x13, kSteepAtSites, 1);
    void* overlay = Clone("MapCell_FlatOverlay", 0x570AB0, 0x110, kOverlaySites, 2);
    void* patch = Clone("AreaMap_ApplyPatch", 0x571110, 0x1CC, kPatchSites, 3);
    void* clut = Clone("Gfx_ClutAdjust", 0x5718F0, 0x13F, nullptr, 0);
    void* slope = Clone("MapView_SlopeAt", 0x5725C0, 0x24, kSlopeSites, 2);
    void* test_fb = Clone("MoveCmd_TestFB", 0x572650, 0x13A, nullptr, 0);
    void* test_fc = Clone("MoveCmd_TestFC", 0x572790, 0x13A, nullptr, 0);
    void* item_at = Clone("MapView_ItemAt", 0x572ED0, 0x98, nullptr, 0);
    const Handler handlers[] = {
        {"D3d_DrawPolyF4", 0x5A0AB0, 0x185, kPolyF4Sites, 5, As<const void*>(&D3d_DrawPolyF4), 0x38, 4, 0xC, false},
        {"D3d_DrawPolyG4", 0x5A1290, 0x22F, kPolyG4Sites, 8, As<const void*>(&D3d_DrawPolyG4), 0x48, 4, 0x10, true},
        {"D3d_DrawLineF3", 0x5A1A00, 0x14C, kLineF3Sites, 5, As<const void*>(&D3d_DrawLineF3), 0x2C, 3, 0xC, false},
    };
    void* draws[3];
    for (int i = 0; i < 3; ++i)
        draws[i] = Clone(handlers[i].name, handlers[i].base, handlers[i].size, handlers[i].sites, handlers[i].n_sites);
    void* set_f4 = Clone("Gpu_SetPolyF4", 0x5A75B0, 0x1A, nullptr, 0);
    void* set_g4 = Clone("Gpu_SetPolyG4", 0x5A7610, 0x1A, nullptr, 0);
    void* set_l3 = Clone("Gpu_SetLineF3", 0x5A7670, 0x17, nullptr, 0);
    void* set_move = Clone("Gpu_SetDrawMove", 0x5A7810, 0x29, nullptr, 0);

    const int n_all = static_cast<int>(sizeof g_all / sizeof g_all[0]);
    static State saved;
    if (RegionBytes(g_all, n_all) > kMaxState) bof3::Fatal("field_misc: the saved regions outgrow the state buffer");
    Capture(g_all, n_all, saved);
    const Callees saved_callees = g;
    g = kStandIns;
    d3d_fuzz::Seed(0x4D4D4D4Du);

    Tally tallies[] = {
        {"AreaMap_BlockedNarrow", 0, 0, 0, 0}, {"AreaMap_TooSteepAt", 0, 0, 0, 0}, {"MapView_SlopeAt", 0, 0, 0, 0},
        {"MapView_ItemAt", 0, 0, 0, 0},        {"MoveCmd_TestFB", 0, 0, 0, 0},     {"MoveCmd_TestFC", 0, 0, 0, 0},
        {"AreaMap_ApplyPatch", 0, 0, 0, 0},    {"MapCell_FlatOverlay", 0, 0, 0, 0}, {"Gfx_ClutAdjust", 0, 0, 0, 0},
        {"D3d_DrawPolyF4", 0, 0, 0, 0},        {"D3d_DrawPolyG4", 0, 0, 0, 0},     {"D3d_DrawLineF3", 0, 0, 0, 0},
        {"Gpu_SetPolyF4", 0, 0, 0, 0},         {"Gpu_SetPolyG4", 0, 0, 0, 0},      {"Gpu_SetLineF3", 0, 0, 0, 0},
        {"Gpu_SetDrawMove", 0, 0, 0, 0},
    };
    FuzzNarrow(tallies[0], narrow, 20000);
    FuzzSteepAt(tallies[1], steep_at, 2000);
    FuzzSlopeAt(tallies[2], slope, 5000);
    FuzzItemAt(tallies[3], item_at, 20000);
    FuzzTileTest(tallies[4], test_fb, Addr(reinterpret_cast<const void*>(&MoveCmd_TestFB)), 10000, 0x900000u);
    FuzzTileTest(tallies[5], test_fc, Addr(reinterpret_cast<const void*>(&MoveCmd_TestFC)), 10000, 0xA00000u);
    FuzzApplyPatch(tallies[6], patch, 20000);
    FuzzOverlay(tallies[7], overlay, 10000);
    FuzzClut(tallies[8], clut, 2000);
    {
        d3d_fuzz::DeviceSwap swap;
        for (int i = 0; i < 3; ++i) FuzzHandler(tallies[9 + i], handlers[i], draws[i], 20000);
    }
    FuzzSetter(tallies[12], set_f4, As<const void*>(&Gpu_SetPolyF4), false, 2000);
    FuzzSetter(tallies[13], set_g4, As<const void*>(&Gpu_SetPolyG4), false, 2000);
    FuzzSetter(tallies[14], set_l3, As<const void*>(&Gpu_SetLineF3), false, 2000);
    FuzzSetter(tallies[15], set_move, As<const void*>(&Gpu_SetDrawMove), true, 5000);

    g = saved_callees;
    Restore(g_all, n_all, saved);

    unsigned bad = 0, rounds = 0;
    for (const Tally& t : tallies) {
        bad += t.bad;
        rounds += t.rounds;
        bof3::Log("shadow      field_misc self-test: %s %u rounds, %u calls out, %u covered, %u MISMATCHES", t.name,
                  t.rounds, t.calls, t.hits, t.bad);
    }
    bof3::Log("shadow      field_misc self-test: %u rounds over 16 functions, %u MISMATCHES", rounds, bad);
    if (bad) bof3::Fatal("group M's functions differ from the original in %u of %u self-test rounds", bad, rounds);
}

}  // namespace field_misc
