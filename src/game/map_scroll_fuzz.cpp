// BOF3X_SHADOW=map_scroll: the start-up differential fuzz of map_scroll.cpp's
// nine functions (docs/map-scroll.md, "Checks").
//
// Each original is byte-copied with EVERY direct call re-aimed at a recording
// stand-in - Field_ViewReset's calls of the three area set-up functions and
// of MapView_PlaceRuns, and AreaMap_SetupEntries' of AreaMap_ClutCycleStart,
// included, so each function is tested alone - and ours runs with the same
// stand-ins through map_scroll::g. AreaMap_SetupEntries calls through
// AreaMap_SetupHandlers, whose 64 entries the mask reaches become 64 numbered
// stand-ins for the fuzz and are put back. A round: random state with each
// branch's boundaries seeded, theirs, the same state again, ours; every byte
// of the state, the stand-ins' log (a count, a hash of every entry and the
// first 48 kept) and the state each stand-in sees compared. The stand-ins
// give back what the real callee would leave for the caller to read, and now
// and then disturb what the caller reads again after the call.
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <initializer_list>
#include <utility>

#include "bof3/symbols.gen.h"
#include "game/map_scroll_callees.h"
#include "hook/detour.h"
#include "hook/log.h"

namespace map_scroll {
namespace {

std::uint32_t Address(const void* p) { return static_cast<std::uint32_t>(reinterpret_cast<std::uintptr_t>(p)); }
std::uint32_t Dword(const unsigned char* p) {
    std::uint32_t v;
    std::memcpy(&v, p, sizeof v);
    return v;
}
unsigned Word(const unsigned char* p) {
    std::uint16_t v;
    std::memcpy(&v, p, sizeof v);
    return v;
}
void SetWord(unsigned char* p, unsigned v) {
    const auto w = static_cast<std::uint16_t>(v);
    std::memcpy(p, &w, sizeof w);
}
void SetDword(unsigned char* p, std::uint32_t v) { std::memcpy(p, &v, sizeof v); }

std::uint32_t g_rng;
std::uint32_t Next() {
    g_rng ^= g_rng << 13;
    g_rng ^= g_rng >> 17;
    g_rng ^= g_rng << 5;
    return g_rng;
}
bool OneIn(unsigned n) { return Next() % n == 0; }
template <class T, unsigned N> T Pick(const T (&a)[N]) { return a[Next() % N]; }

// --- the stand-ins' log -------------------------------------------------------------

constexpr unsigned kKeep = 48, kIds = 256;
struct Log {
    std::uint32_t n, hash;
    std::uint32_t keep[kKeep][4];
    unsigned counts[kIds];
};
Log g_log;
std::uint32_t g_seed;

void Record(std::uint32_t what, std::uint32_t a = 0, std::uint32_t b = 0, std::uint32_t c = 0) {
    if (g_log.n < kKeep) {
        g_log.keep[g_log.n][0] = what;
        g_log.keep[g_log.n][1] = a;
        g_log.keep[g_log.n][2] = b;
        g_log.keep[g_log.n][3] = c;
    }
    for (const std::uint32_t v : {what, a, b, c}) g_log.hash = (g_log.hash ^ v) * 0x01000193u + 0x9E3779B9u;
    ++g_log.n;
    ++g_log.counts[what % kIds];
}
// Deterministic in the call's position: the same on both sides while the
// calls agree.
std::uint32_t Hash() {
    std::uint32_t h = (g_seed + g_log.n) * 0x9E3779B1u;
    h ^= h >> 15;
    h *= 0x85EBCA6Bu;
    h ^= h >> 13;
    h *= 0xC2B2AE35u;
    h ^= h >> 16;
    return h;
}
std::uint32_t Sum(const void* p, unsigned n) {
    std::uint32_t h = 0x811C9DC5u;
    for (unsigned i = 0; i < n; ++i) h = (h ^ static_cast<const unsigned char*>(p)[i]) * 0x01000193u;
    return h;
}
std::uint32_t Pack(const short* v) {
    return static_cast<std::uint16_t>(v[0]) | static_cast<std::uint32_t>(static_cast<std::uint16_t>(v[1])) << 16;
}

// --- the state --------------------------------------------------------------------------

struct Region { unsigned char* at; unsigned size; };
template <class T> Region R(T& v) { return {reinterpret_cast<unsigned char*>(&v), sizeof v}; }
Region R(void* at, unsigned size) { return {static_cast<unsigned char*>(at), size}; }

// What a stand-in may hash to show the state it was called in: a caller that
// stores later, or earlier, than the original is seen at the call.
const Region* g_watch = nullptr;
unsigned g_watch_n = 0;
std::uint32_t Watch() {
    std::uint32_t h = 0;
    for (unsigned i = 0; i < g_watch_n; ++i) h = h * 31u + Sum(g_watch[i].at, g_watch[i].size);
    return h;
}

constexpr unsigned kStateMax = 0x28000;
unsigned char g_saved[kStateMax], g_input[kStateMax], g_out[2][kStateMax];
Log g_logs[2];

unsigned Total(const Region* r, unsigned n) {
    unsigned total = 0;
    for (unsigned i = 0; i < n; ++i) {
        total += r[i].size;
        for (unsigned j = 0; j < i; ++j)
            if (r[i].at < r[j].at + r[j].size && r[j].at < r[i].at + r[i].size)
                bof3::Fatal("map_scroll self-test: state regions 0x%08X and 0x%08X overlap",
                            static_cast<unsigned>(Address(r[i].at)), static_cast<unsigned>(Address(r[j].at)));
    }
    if (total > kStateMax) bof3::Fatal("map_scroll self-test: state of 0x%X bytes", total);
    return total;
}
void Capture(const Region* r, unsigned n, unsigned char* out) {
    for (unsigned i = 0; i < n; out += r[i].size, ++i) std::memcpy(out, r[i].at, r[i].size);
}
void Apply(const Region* r, unsigned n, const unsigned char* in) {
    for (unsigned i = 0; i < n; in += r[i].size, ++i) std::memcpy(r[i].at, in, r[i].size);
}
void Randomize(const Region* r, unsigned n) {
    for (unsigned i = 0; i < n; ++i)
        for (unsigned k = 0; k < r[i].size; ++k) r[i].at[k] = static_cast<unsigned char>(Next());
}

// Runs `theirs` then `ours` from the state as it stands; true if the state or
// the log differ.
template <class Theirs, class Ours>
bool Pair(const char* name, unsigned round, const Region* r, unsigned n, Theirs&& theirs, Ours&& ours, unsigned& bad) {
    const unsigned total = Total(r, n);
    Capture(r, n, g_input);
    g_seed = Next();
    for (int pass = 0; pass < 2; ++pass) {
        Apply(r, n, g_input);
        std::memset(&g_log, 0, sizeof g_log);
        if (pass == 0) theirs();
        else ours();
        Capture(r, n, g_out[pass]);
        g_logs[pass] = g_log;
    }
    const bool log_differs = g_logs[0].n != g_logs[1].n || g_logs[0].hash != g_logs[1].hash;
    const bool state_differs = std::memcmp(g_out[0], g_out[1], total) != 0;
    if (!log_differs && !state_differs) return false;
    if (++bad <= 8) {
        if (state_differs) {
            unsigned at = 0;
            while (g_out[0][at] == g_out[1][at]) ++at;
            unsigned region = 0, base = 0;
            while (at >= base + r[region].size) base += r[region++].size;
            bof3::Log("shadow      map_scroll %s MISMATCH: round %u, state region %u (0x%08X) +0x%X: %02X / %02X", name,
                      round, region, static_cast<unsigned>(Address(r[region].at)), at - base, g_out[0][at],
                      g_out[1][at]);
        } else {
            unsigned at = 0;
            const unsigned kept = g_logs[0].n < kKeep ? g_logs[0].n : kKeep;
            while (at < kept && std::memcmp(g_logs[0].keep[at], g_logs[1].keep[at], 16) == 0) ++at;
            const std::uint32_t* a = at < kKeep ? g_logs[0].keep[at] : g_logs[0].keep[0];
            const std::uint32_t* b = at < kKeep ? g_logs[1].keep[at] : g_logs[1].keep[0];
            bof3::Log("shadow      map_scroll %s MISMATCH: round %u, log of %u / %u calls, first difference at entry "
                      "%u: %X(%X, %X, %X) / %X(%X, %X, %X)", name, round, g_logs[0].n, g_logs[1].n, at, a[0], a[1],
                      a[2], a[3], b[0], b[1], b[2], b[3]);
        }
    }
    return true;
}

void Report(const char* name, unsigned rounds, unsigned bad, const char* detail) {
    bof3::Log("shadow      map_scroll %s self-test: %u rounds (%s), %u MISMATCHES", name, rounds, detail, bad);
    if (bad) bof3::Fatal("%s differs from the original in %u of %u self-test rounds", name, bad, rounds);
}

// --- the stand-ins ------------------------------------------------------------------------

std::uint32_t CellOff(const unsigned char* p) { return Address(p) - Address(MapView_CellItems); }
std::uint32_t ItemOff(const unsigned char* p) { return Address(p) - Address(DrawItems); }
std::uint32_t BlockOff(const unsigned char* p) { return Address(p) - Address(AreaMap_Header); }

// The shifts read the ring's heads again during their walks: a stand-in may
// move one (inside the ring, so the walk stays in MapView_CellItems).
bool g_disturb_heads = false;
void DisturbHeads(std::uint32_t h) {
    if (!g_disturb_heads) return;
    if (h % 16 == 1) MapView_Column = static_cast<short>((h >> 8) % 0x1C);
    if (h % 16 == 2) MapView_Row = static_cast<short>((h >> 8) % 0x38);
}
std::uint32_t Heads() {
    return static_cast<std::uint16_t>(MapView_Row) | static_cast<std::uint32_t>(static_cast<std::uint16_t>(MapView_Column)) << 16;
}

// The real one releases the cell's draw items and clears its word.
void __cdecl StubReleaseCell(unsigned char* cell) {
    Record(1, CellOff(cell), Word(cell + 2), Heads());
    const std::uint32_t h = Hash();
    if (h % 4 != 0) SetWord(cell + 2, 0);
    DisturbHeads(h >> 3);
}
// The real one writes the cell's map x and y.
void __cdecl StubCellToMap(int row, int col, unsigned char* out) {
    Record(2, static_cast<std::uint32_t>(row), static_cast<std::uint32_t>(col), CellOff(out));
    Record(2, Pack(MapView_Origin), Word(out + 2), Heads());
    const std::uint32_t h = Hash();
    out[0] = static_cast<unsigned char>(h);
    out[1] = static_cast<unsigned char>(h >> 8);
    DisturbHeads(h >> 5);
}

void __cdecl StubGeomScreen(long h) { Record(3, static_cast<std::uint32_t>(h), Watch()); }
void __cdecl StubGeomOffset(long x, long y) { Record(4, static_cast<std::uint32_t>(x), static_cast<std::uint32_t>(y), Watch()); }
void __cdecl StubBackColor(long r, long gr, long b) {
    Record(5, static_cast<std::uint32_t>(r), static_cast<std::uint32_t>(gr), static_cast<std::uint32_t>(b));
}
// The real ones write the primitive's code and length bytes; these also now
// and then write the two words the caller clears after them.
void __cdecl StubSetPolyFT4(unsigned char* prim) {
    Record(6, ItemOff(prim), Word(prim + 0x36), Word(prim + 0x46));
    const std::uint32_t h = Hash();
    prim[3] = static_cast<unsigned char>(h);
    prim[7] = static_cast<unsigned char>(h >> 8);
    if (h % 8 == 0) SetWord(prim + 0x36, h >> 16);
}
void __cdecl StubSetShadeTex(unsigned char* prim, unsigned tge) {
    Record(7, ItemOff(prim), tge);
    const std::uint32_t h = Hash();
    prim[7] = static_cast<unsigned char>(prim[7] ^ (h & 3));
    if (h % 8 == 1) SetWord(prim + 0x46, h >> 12);
}
// The result's upper half is noise the caller must drop; the input flags are
// read again after the call.
long __cdecl StubElevation(long x, long y) {
    Record(8, static_cast<std::uint32_t>(x), static_cast<std::uint32_t>(y), Watch());
    const std::uint32_t h = Hash();
    if (h % 4 == 0) Field_InputFlags = static_cast<unsigned char>(h >> 8);
    switch ((h >> 4) % 4) {
        case 0: return static_cast<long>(h);
        case 1: return static_cast<long>(h | 0x8000u);
        case 2: return static_cast<long>(h & 0xFFFF7FFFu);
        default: return static_cast<long>(static_cast<short>(h >> 16));
    }
}
void __cdecl StubBake() { Record(9, Watch()); }
void __cdecl StubSetup() { Record(10, Watch()); }
void __cdecl StubPlaceRuns() { Record(11, Watch()); }

// AreaMap_SetupEntries': AreaMap_PatchBase is read after the palette call,
// and a patch entry's length after its call.
unsigned short g_alt_patch_base;
void __cdecl StubClutStart() {
    Record(12, Cond_ByteFF, AreaMap_PatchBase);
    if (Hash() % 4 == 0) AreaMap_PatchBase = g_alt_patch_base;
}
// An entry marked (bit 15 of its low word) is followed by a one-dword entry
// or the list's end, so lengthening it from 0 to 1 lands on an entry.
void __cdecl StubApplyPatch(const unsigned char* entry) {
    Record(13, BlockOff(entry), Dword(entry), Cond_ByteFF);
    const std::uint32_t d = Dword(entry);
    if ((d & 0x8000u) && (d >> 16) == 0 && Hash() % 3 == 0)
        SetDword(const_cast<unsigned char*>(entry), d | 0x10000u);
}

const Callees kStubs = {
    StubReleaseCell, StubCellToMap,
    StubGeomScreen, StubGeomOffset, StubBackColor, StubSetPolyFT4, StubSetShadeTex, StubElevation,
    StubBake, StubSetup, StubPlaceRuns,
    StubClutStart, StubApplyPatch,
};

// A stand-in per entry of AreaMap_SetupHandlers the mask reaches. One may
// lengthen its entry's step from 1 to 2, read after the call: the list is laid
// out so that either step lands on an entry or on a zero.
template <unsigned K> void __cdecl SetupStandIn(const unsigned char* entry) {
    Record(0x40 + K, BlockOff(entry), entry[2], Cond_ByteFF);
    if (entry[2] == 1 && Hash() % 4 == 0) const_cast<unsigned char*>(entry)[2] = 2;
}
template <unsigned... K> void FillHandlers(std::integer_sequence<unsigned, K...>) {
    ((AreaMap_SetupHandlers[K] = static_cast<unsigned long>(Address(reinterpret_cast<const void*>(&SetupStandIn<K>)))), ...);
}

const void* StubFor(std::uint32_t target) {
    const auto f = [](auto p) { return reinterpret_cast<const void*>(p); };
    switch (target) {
        case 0x56FC00: return f(kStubs.release_cell);
        case 0x56F910: return f(kStubs.cell_to_map);
        case 0x5A7B00: return f(kStubs.geom_screen);
        case 0x5A7AE0: return f(kStubs.geom_offset);
        case 0x5A7B60: return f(kStubs.back_color);
        case 0x5A75D0: return f(kStubs.set_poly_ft4);
        case 0x5A77A0: return f(kStubs.set_shade_tex);
        case 0x5720C0: return f(kStubs.elevation);
        case 0x56FAD0: return f(kStubs.bake_patches);
        case 0x571720: return f(kStubs.setup_entries);
        case 0x571FF0: return f(kStubs.place_runs);
        case 0x5717B0: return f(kStubs.clut_start);
        case 0x571110: return f(kStubs.apply_patch);
        default: bof3::Fatal("map_scroll: no stand-in for a call to 0x%X", static_cast<unsigned>(target));
    }
}

// The copies and their calls, by capstone 2026-09-22 (every E8 in each
// function; no jump leaves any of them, none has a jump table).
struct Call { std::uint32_t offset, target; };
constexpr Call kColumnPrevCalls[] = {{0x31, 0x56FC00}, {0x3A, 0x56F910}};
constexpr Call kColumnNextCalls[] = {{0x40, 0x56FC00}, {0x49, 0x56F910}};
constexpr Call kRowsPrevCalls[] = {{0x49, 0x56FC00}, {0x51, 0x56F910}};
constexpr Call kRowsNextCalls[] = {{0x55, 0x56FC00}, {0x64, 0x56F910}};
constexpr Call kResetCalls[] = {{0x08, 0x5A7B00},  {0x14, 0x5A7AE0},  {0x1F, 0x5A7B60},  {0x34, 0x5A75D0},
                                {0x3B, 0x5A77A0},  {0x1A0, 0x5720C0}, {0x235, 0x56F910}, {0x27E, 0x56FAD0},
                                {0x283, 0x571720}, {0x288, 0x571FF0}};
constexpr Call kSetupCalls[] = {{0x46, 0x5717B0}, {0x68, 0x571110}};

template <unsigned N>
void* Clone(const char* name, std::uint32_t base, std::uint32_t size, const Call (&calls)[N]) {
    bof3::CloneCall re_aimed[N];
    for (unsigned i = 0; i < N; ++i) re_aimed[i] = {calls[i].offset, StubFor(calls[i].target)};
    return bof3::CloneOriginal(name, base, size, re_aimed, static_cast<int>(N));
}

using Fn = void(__cdecl*)();

// --- the four shifts ------------------------------------------------------------------------

void SelfTestShift(const char* name, Fn theirs, Fn ours, std::uint32_t seed) {
    constexpr unsigned kRounds = 20000;
    const Region r[] = {R(MapView_CellItems, MapView_CellItems_count), R(&MapView_Column, 6),   // to MapView_Row
                        R(MapView_Origin, 4)};
    constexpr unsigned n = sizeof r / sizeof r[0];
    Capture(r, n, g_saved);
    g_rng = seed;
    g_disturb_heads = true;
    unsigned bad = 0, column_edge = 0, row_edge = 0;
    for (unsigned round = 0; round < kRounds; ++round) {
        Randomize(r, n);
        static const short kColumns[] = {0, 1, 0x1A, 0x1B};
        static const short kRows[] = {0, 1, 0x36, 0x37};
        MapView_Column = OneIn(2) ? Pick(kColumns) : static_cast<short>(Next() % 0x1C);
        MapView_Row = OneIn(2) ? Pick(kRows) : static_cast<short>(Next() % 0x38);
        static const short kOrigin[] = {0, -1, 0x7FFF, -0x8000, 1};
        if (OneIn(4)) MapView_Origin[Next() % 2] = Pick(kOrigin);
        column_edge += MapView_Column == 0 || MapView_Column == 0x1B;
        row_edge += MapView_Row == 0 || MapView_Row == 0x37;
        Pair(name, round, r, n, theirs, ours, bad);
    }
    g_disturb_heads = false;
    Apply(r, n, g_saved);
    char detail[200];
    std::snprintf(detail, sizeof detail, "%u with the head column at 0 or 0x1B, %u with the head row at 0 or 0x37",
                  column_edge, row_edge);
    Report(name, kRounds, bad, detail);
}

// --- MapView_PlaceRuns --------------------------------------------------------------------------

void SelfTestPlaceRuns(Fn theirs) {
    constexpr unsigned kRounds = 20000, kBlock = 0x2000;
    const Region r[] = {R(MapView_Cells, MapView_Cells_count * 2), R(AreaMap_Header, kBlock), R(MapView_Origin, 4),
                        R(&MapView_Column, 6)};
    constexpr unsigned n = sizeof r / sizeof r[0];
    Capture(r, n, g_saved);
    g_rng = 0x571FF001u;
    unsigned bad = 0, runs = 0, placed = 0, edges = 0;
    for (unsigned round = 0; round < kRounds; ++round) {
        Randomize(r, n);
        static const short kColumns[] = {0, 1, 0x1A, 0x1B};
        static const short kRows[] = {0, 1, 0x36, 0x37};
        MapView_Column = OneIn(2) ? Pick(kColumns) : static_cast<short>(Next() % 0x1C);
        MapView_Row = OneIn(2) ? Pick(kRows) : static_cast<short>(Next() % 0x38);
        MapView_Origin[0] = static_cast<short>(0x20 + Next() % 0x40);
        MapView_Origin[1] = static_cast<short>(0x40 + Next() % 0x40);
        const unsigned base = 0x100 + Next() % 0x300;   // dwords: the list inside the block's first 0x1400 bytes
        AreaMap_CellBase = (Next() & 0xFFFF0000u) | base;
        const unsigned count = OneIn(8) ? 0 : Next() % 30;
        unsigned char* p = AreaMap_Header + base * 4;
        for (unsigned k = 0; k < count; ++k) {
            // A view row s and doubled column d, often at the edges of 0..0x37.
            static const int kEdge[] = {-1, 0, 1, 0x36, 0x37, 0x38};
            const int s = OneIn(2) ? Pick(kEdge) : static_cast<int>(Next() % 0x40) - 3;
            int d = OneIn(2) ? Pick(kEdge) : static_cast<int>(Next() % 0x40) - 3;
            if ((s + d) & 1) d += OneIn(2) ? 1 : -1;
            unsigned x = static_cast<unsigned>(MapView_Origin[0] + (s + d) / 2);
            unsigned y = static_cast<unsigned>(MapView_Origin[1] + (s - d) / 2);
            if (OneIn(8)) { x = Next(); y = Next(); }
            const unsigned step = 1 + Next() % 6;
            SetDword(p, (x & 0xFFu) << 8 | (y & 0xFFu) | step << 16);
            edges += (s == -1 || s == 0 || s == 0x37 || s == 0x38 || d == -1 || d == 0 || d == 0x37 || d == 0x38);
            p += step * 4;
        }
        SetDword(p, 0);
        runs += count;
        Pair("MapView_PlaceRuns", round, r, n, theirs, [] { MapView_PlaceRuns(); }, bad);
        for (unsigned i = 0; i < MapView_Cells_count; ++i) placed += Word(g_out[0] + i * 2) != 0;
    }
    Apply(r, n, g_saved);
    char detail[200];
    std::snprintf(detail, sizeof detail, "%u runs, %u placed in the view, %u aimed at an edge of it", runs, placed,
                  edges);
    Report("MapView_PlaceRuns", kRounds, bad, detail);
}

// --- Field_ViewReset ---------------------------------------------------------------------------

void SelfTestViewReset(Fn theirs) {
    constexpr unsigned kRounds = 600;
    const Region r[] = {
        R(DrawItems, DrawItems_count),
        R(Camera_Angles, 8),                 // with Cond_AngleFB and the word after it
        R(&MapView_BuildFlags, 1),
        R(&MapView_FocusX, 0x12),            // FocusZ, Elevation, Column, HeightScale, to MapView_Row
        R(Camera_AnglesDrawn, 8),
        R(&AreaMap_Bytes, 4),
        R(&DrawTable_Count, 3),              // and Field_InputFlags
        R(&Cond_ByteFE, 1),
        R(&Field_Kind2Z, 10),                // Kind2X, to MapView_Redraw
        R(MapView_Origin, 4),
        R(&Camera_ShiftX, 4),                // and Camera_ShiftY
        R(&Camera_Distance, 2),
        R(AreaMap_Header, 0x30),             // AreaMap_BytesBase, AreaMap_Word1E
        R(&MapView_ScrollX, 4),              // and MapView_ElevationOffset
        R(&MoveScript_F3Divisor, 2),
        R(&MoveScript_FAWord, 2),
        R(MapView_CellItems, MapView_CellItems_count),
        R(DrawItemPool_Free, DrawItemPool_Free_count * 2),
        R(&DrawItemPool_Top, 2),
    };
    constexpr unsigned n = sizeof r / sizeof r[0];
    // Everything but the quads and the cells, which are checked call by call.
    Region watch[n];
    unsigned nw = 0;
    watch[nw++] = R(DrawItems, 0x90);
    for (unsigned i = 1; i < n; ++i)
        if (r[i].at != MapView_CellItems) watch[nw++] = r[i];
    g_watch = watch;
    g_watch_n = nw;
    Capture(r, n, g_saved);
    g_rng = 0x56F67001u;
    unsigned bad = 0, called = 0, fixed = 0, negative = 0;
    for (unsigned round = 0; round < kRounds; ++round) {
        Randomize(r, n);
        if (OneIn(2)) Field_InputFlags = static_cast<unsigned char>(Field_InputFlags & ~8u);
        else Field_InputFlags = static_cast<unsigned char>(Field_InputFlags | 8u);
        if (OneIn(2)) AreaMap_Word1E = 0;
        static const long kPos[] = {0, -1, 0x7FFFFFFF, static_cast<long>(0x80000000u), 0x00FF8000, -0x100};
        if (OneIn(4)) Field_Kind2X = Pick(kPos);
        if (OneIn(4)) Field_Kind2Z = Pick(kPos);
        called += !(Field_InputFlags & 8);
        fixed += AreaMap_Word1E != 0;
        Pair("Field_ViewReset", round, r, n, theirs, [] { Field_ViewReset(); }, bad);
        const Log& l = g_logs[0];
        if (l.counts[8] != 0) {
            const long e = static_cast<long>(Dword(g_out[0] + DrawItems_count + 8 + 1 + 8));   // MapView_Elevation
            negative += e < 0;
        }
    }
    g_watch = nullptr;
    g_watch_n = 0;
    Apply(r, n, g_saved);
    char detail[200];
    std::snprintf(detail, sizeof detail, "%u with the elevation call, %u of them below 0; %u with AreaMap_Word1E set",
                  called, negative, fixed);
    Report("Field_ViewReset", kRounds, bad, detail);
}

// --- AreaMap_BakePatches --------------------------------------------------------------------------

// The block: width and height up to 16, the offset 0x10..0x40 dwords, the cell
// words random below 0x200 and the texture run's first 0x900 dwords small in
// both halves (a record just outside the grid reads a run word as its tile),
// so every write stays below dword 0x1300 - even a planted bug's that writes
// a record's own first dword; the patch list from dword 0x1400. The one
// write aimed at the list is the entry below that shortens itself.
constexpr unsigned kBakeBlock = 0x6000;

unsigned char* Block(unsigned dword) { return AreaMap_Header + dword * 4; }

void SelfTestBake(Fn theirs) {
    constexpr unsigned kRounds = 12000;
    const Region r[] = {R(AreaMap_Header, kBakeBlock)};
    constexpr unsigned n = 1;
    Capture(r, n, g_saved);
    g_rng = 0x56FAD001u;
    unsigned bad = 0, entries = 0, baked = 0, tricky = 0, skipped_top = 0;
    for (unsigned round = 0; round < kRounds; ++round) {
        Randomize(r, n);
        const unsigned width = 1 + Next() % 16, height = 1 + Next() % 16, offset = 0x10 + Next() % 0x31;
        AreaMap_Header[0] = static_cast<unsigned char>(width);
        AreaMap_Header[1] = static_cast<unsigned char>(height);
        SetWord(AreaMap_Header + 2, offset);
        for (unsigned i = 0; i < width * height + 2 * width + 2; ++i)
            SetWord(AreaMap_Header + (offset * 2 + i) * 2, OneIn(6) ? 0 : Next() % 0x200);
        const unsigned run = offset + (height * width + 1) / 2;
        for (unsigned i = 0; i < 0x900; ++i) SetDword(Block(run + i), (Next() % 0x400) << 16 | Next() % 0x400);
        const unsigned list = 0x1400 + Next() % 0x100;
        SetWord(reinterpret_cast<unsigned char*>(&AreaMap_PatchBase), list);
        unsigned at = list;
        auto record = [&](unsigned k, unsigned first_top) {
            // Never cell (0, 0): its tile belongs to the entry below that shortens itself.
            const unsigned y = OneIn(8) ? height : 1 + Next() % height, x = OneIn(8) ? width : Next() % width;
            const unsigned top = k == 0 ? first_top : Next() % 0x100;
            SetDword(Block(at), top << 24 | (Next() % 0x100) << 16 | x << 8 | y);
            SetDword(Block(at + 1), (Next() % 0x400) << 16 | Next() % 0x400);
            SetDword(Block(at + 2), Next());
        };
        if (OneIn(4)) {
            // The entry that shortens itself: its first record's tile aims the
            // write at the entry's own header, turning a length of 6 into 3;
            // the next entry starts where the shorter length says.
            ++tricky;
            const unsigned half = (height * width + 1) / 2, nib = Next() % 16;
            SetDword(Block(at), 6u << 16 | 0x8001u);
            SetDword(Block(at + 1), static_cast<std::uint32_t>(OneIn(2)) << 24 | nib << 16);   // cell (0, 0)
            SetDword(Block(at + 2), 3u << 16 | 0x8000u);
            SetDword(Block(at + 3), Next());
            SetWord(AreaMap_Header + offset * 4, at - nib - half - offset);   // cell (0, 0)'s tile
            at += 4;
        }
        const unsigned count = OneIn(8) ? 0 : 1 + Next() % 6;
        for (unsigned e = 0; e < count; ++e) {
            static const unsigned kCode[] = {0x8001, 0x8001, 0x8001, 0x8000, 0x8003, 0x0001, 0x9001, 0xC001};
            static const unsigned kTop[] = {0, 0, 1, 1, 2, 3, 0xFF};
            static const unsigned kLength[] = {0, 1, 2, 3, 3, 4, 5, 6, 6, 9};
            const unsigned code = OneIn(8) ? Next() % 0x10000 : Pick(kCode);
            const unsigned length = Pick(kLength), first_top = Pick(kTop);
            SetDword(Block(at), length << 16 | code);
            if (length == 0 && code == 0) SetDword(Block(at), 0x8000u);
            ++at;
            for (unsigned k = 0; k < length; k += 3) {
                record(k, first_top);
                at += 3;
            }
            at -= (3 - length % 3) % 3;   // the next entry follows the length, not the records
            entries += 1;
            skipped_top += (code == 0x8001 && first_top >= 2);
        }
        for (unsigned k = 0; k < 4; ++k) SetDword(Block(at + k), 0);
        Pair("AreaMap_BakePatches", round, r, n, theirs, [] { AreaMap_BakePatches(); }, bad);
        // Entries baked: their code turned from 0x8001 to 0x8000.
        for (unsigned d = list; d < at; ++d)
            if (Word(g_input + d * 4) == 0x8001 && Word(g_out[0] + d * 4) == 0x8000) ++baked;
    }
    Apply(r, n, g_saved);
    char detail[200];
    std::snprintf(detail, sizeof detail,
                  "%u entries, %u baked, %u refused by the first record's top byte; %u rounds with an entry that "
                  "shortens itself",
                  entries, baked, skipped_top, tricky);
    Report("AreaMap_BakePatches", kRounds, bad, detail);
}

// --- AreaMap_ClutCycleStart ---------------------------------------------------------------------

void SelfTestClutStart(Fn theirs) {
    constexpr unsigned kRounds = 30000;
    const Region r[] = {R(Gfx_ClutStrip, Gfx_ClutStrip_count * 2), R(&Gfx_ClutStripDirty, 1), R(&Frame_Counter, 4),
                        R(AreaMap_Header, 0x1000)};
    constexpr unsigned n = sizeof r / sizeof r[0];
    Capture(r, n, g_saved);
    g_rng = 0x5717B001u;
    unsigned bad = 0, cycles = 0, others = 0, copied = 0, exact = 0;
    for (unsigned round = 0; round < kRounds; ++round) {
        Randomize(r, n);
        const unsigned base = 0x40 + Next() % 0xC0;
        AreaMap_EntryBase = static_cast<unsigned short>(base);
        unsigned at = base;
        const unsigned count = OneIn(8) ? 0 : 1 + Next() % 5;
        unsigned aimed = 0, aimed_period = 1;
        for (unsigned e = 0; e < count; ++e) {
            static const unsigned kKind[] = {0x80, 0x80, 0x80, 0x00, 0x81, 0x7F, 0xFF};
            static const unsigned kPeriod[] = {1, 2, 3, 0xFE, 0xFF};
            const unsigned kind = OneIn(8) ? 1 + Next() % 0xFF : Pick(kKind);
            const unsigned period = OneIn(3) ? Pick(kPeriod) : 1 + Next() % 0xFF;
            const unsigned frames = Next() % 7;
            SetDword(Block(at), kind << 24 | (frames + 2) << 16 | (Next() % 0x100) << 8 | period);
            unsigned t = 1;
            for (unsigned k = 0; k < frames; ++k) {
                t += OneIn(4) ? 0 : Next() % 40;
                if (t > 0xFE) t = 0xFE;
                SetDword(Block(at + 1 + k), t << 24 | (Next() % 0x200) << 12 | Next() % 0x200);
                if (kind == 0x80 && OneIn(3)) {
                    aimed = t + (OneIn(2) ? 0 : OneIn(2) ? 1 : 0xFFFFFFFFu);
                    aimed_period = period;
                }
            }
            SetDword(Block(at + 1 + frames), 0xFF000000u | (Next() % 0x200) << 12 | Next() % 0x200);
            at += frames + 2;
            if (kind == 0x80) ++cycles;
            else ++others;
        }
        SetDword(Block(at), 0);
        // The frame counter at a listed frame, one either side, or anywhere;
        // the periods differ, so it is aimed at one cycle.
        if (aimed != 0) Frame_Counter = aimed - 1 + aimed_period * (Next() % 0x10000);
        Gfx_ClutStripDirty = static_cast<unsigned char>(OneIn(4) ? 1 : 0);
        const bool clean = Gfx_ClutStripDirty == 0;
        Pair("AreaMap_ClutCycleStart", round, r, n, theirs, [] { AreaMap_ClutCycleStart(); }, bad);
        if (clean && g_out[0][Gfx_ClutStrip_count * 2] == 1) ++copied;
        exact += aimed != 0;
    }
    Apply(r, n, g_saved);
    char detail[200];
    std::snprintf(detail, sizeof detail,
                  "%u cycle entries (top byte 0x80), %u other kinds; %u rounds copied a row into a clean strip; %u "
                  "with the counter aimed at a listed frame",
                  cycles, others, copied, exact);
    Report("AreaMap_ClutCycleStart", kRounds, bad, detail);
}

// --- AreaMap_SetupEntries -----------------------------------------------------------------------

void BuildPatchList(unsigned at, unsigned& patches) {
    const unsigned count = OneIn(6) ? 0 : 1 + Next() % 6;
    unsigned lengths[7];
    for (unsigned e = 0; e < count; ++e) lengths[e] = OneIn(2) ? 0 : 1 + Next() % 3;
    for (unsigned e = 0; e < count; ++e) {
        const unsigned length = lengths[e];
        unsigned code = Next() % 0x10000 & ~0x8000u;
        if (code == 0) code = 1;
        // Marked for lengthening only when the dword after it starts an entry
        // of one dword or the end.
        const bool next_short = e + 1 == count || lengths[e + 1] == 0;
        if (length == 0 && next_short && OneIn(2)) code |= 0x8000u;
        SetDword(Block(at), length << 16 | code);
        for (unsigned k = 0; k < length; ++k) SetDword(Block(at + 1 + k), Next());
        at += length + 1;
        ++patches;
    }
    for (unsigned k = 0; k < 3; ++k) SetDword(Block(at + k), 0);
}

void SelfTestSetup(Fn theirs) {
    constexpr unsigned kRounds = 30000;
    const Region r[] = {R(AreaMap_Header, 0x2000), R(&Cond_ByteFF, 1), R(&Field_StatusBits, 1)};
    constexpr unsigned n = sizeof r / sizeof r[0];
    static unsigned long saved_handlers[AreaMap_SetupHandlers_count];
    std::memcpy(saved_handlers, AreaMap_SetupHandlers, sizeof saved_handlers);
    FillHandlers(std::make_integer_sequence<unsigned, AreaMap_SetupHandlers_count>());
    g_watch = nullptr;
    Capture(r, n, g_saved);
    g_rng = 0x57172001u;
    unsigned bad = 0, handled = 0, patched = 0, entries = 0, patches = 0;
    for (unsigned round = 0; round < kRounds; ++round) {
        Randomize(r, n);
        const unsigned setup = 0x100 + Next() % 0x100;
        SetWord(reinterpret_cast<unsigned char*>(&AreaMap_SetupBase), setup);
        const unsigned count = OneIn(8) ? 0 : 1 + Next() % 8;
        for (unsigned k = 0; k < count; ++k) {
            unsigned char* e = Block(setup + k);
            e[2] = 1;
            if (Dword(e) == 0) e[0] = 1;
        }
        std::memset(Block(setup + count), 0, 12);
        entries += count;
        const unsigned patch = 0x300 + Next() % 0x80;
        AreaMap_PatchBase = static_cast<unsigned short>(patch);
        BuildPatchList(patch, patches);
        g_alt_patch_base = static_cast<unsigned short>(0x400 + Next() % 0x80);
        BuildPatchList(g_alt_patch_base, patches);
        Pair("AreaMap_SetupEntries", round, r, n, theirs, [] { AreaMap_SetupEntries(); }, bad);
        handled += g_logs[0].n - g_logs[0].counts[12] - g_logs[0].counts[13];
        patched += g_logs[0].counts[13];
    }
    std::memcpy(AreaMap_SetupHandlers, saved_handlers, sizeof saved_handlers);
    Apply(r, n, g_saved);
    char detail[200];
    std::snprintf(detail, sizeof detail, "%u set-up entries laid, %u handler calls; %u patch entries laid, %u patch calls",
                  entries, handled, patches, patched);
    Report("AreaMap_SetupEntries", kRounds, bad, detail);
}

}  // namespace

void SelfTest() {
    void* const column_prev = Clone("MapView_ShiftColumnPrev", bof3::addr::MapView_ShiftColumnPrev, 0x83, kColumnPrevCalls);
    void* const column_next = Clone("MapView_ShiftColumnNext", bof3::addr::MapView_ShiftColumnNext, 0x71, kColumnNextCalls);
    void* const rows_prev = Clone("MapView_ShiftRowsPrev", bof3::addr::MapView_ShiftRowsPrev, 0x9D, kRowsPrevCalls);
    void* const rows_next = Clone("MapView_ShiftRowsNext", bof3::addr::MapView_ShiftRowsNext, 0xA1, kRowsNextCalls);
    void* const place_runs = bof3::CloneOriginal("MapView_PlaceRuns", bof3::addr::MapView_PlaceRuns, 0xCE);
    void* const reset = Clone("Field_ViewReset", bof3::addr::Field_ViewReset, 0x291, kResetCalls);
    void* const bake = bof3::CloneOriginal("AreaMap_BakePatches", bof3::addr::AreaMap_BakePatches, 0xF8);
    void* const setup = Clone("AreaMap_SetupEntries", bof3::addr::AreaMap_SetupEntries, 0x83, kSetupCalls);
    void* const clut = bof3::CloneOriginal("AreaMap_ClutCycleStart", bof3::addr::AreaMap_ClutCycleStart, 0xC6);
    g = kStubs;
    SelfTestShift("MapView_ShiftColumnPrev", reinterpret_cast<Fn>(column_prev), [] { MapView_ShiftColumnPrev(); }, 0x56E9A001u);
    SelfTestShift("MapView_ShiftColumnNext", reinterpret_cast<Fn>(column_next), [] { MapView_ShiftColumnNext(); }, 0x56EA3001u);
    SelfTestShift("MapView_ShiftRowsPrev", reinterpret_cast<Fn>(rows_prev), [] { MapView_ShiftRowsPrev(); }, 0x56EAB001u);
    SelfTestShift("MapView_ShiftRowsNext", reinterpret_cast<Fn>(rows_next), [] { MapView_ShiftRowsNext(); }, 0x56EB5001u);
    SelfTestPlaceRuns(reinterpret_cast<Fn>(place_runs));
    SelfTestViewReset(reinterpret_cast<Fn>(reset));
    SelfTestBake(reinterpret_cast<Fn>(bake));
    SelfTestClutStart(reinterpret_cast<Fn>(clut));
    SelfTestSetup(reinterpret_cast<Fn>(setup));
    g = kOriginals;
}

}  // namespace map_scroll
