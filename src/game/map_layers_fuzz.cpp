// BOF3X_SHADOW=map_layers: the start-up differential fuzz of map_layers.cpp's
// six functions (docs/map-layers.md, "Checks").
//
// Each original is byte-copied with EVERY call re-aimed at a recording
// stand-in - AreaMap_Frame's call of MapView_Build and MapView_Build's of
// MapView_CellTextures included, so each function is tested alone - and ours
// runs with the same stand-ins through map_layers::g. AreaMap_HeaderPass
// calls through AreaMap_EntryHandlers, whose 128 entries the mask can reach
// become 128 numbered stand-ins for the fuzz and are put back. A round: random
// state with each branch's boundaries seeded, theirs, the same state again,
// ours; every byte of the state, the stand-ins' log (a count, a hash of every
// entry and the first 48 kept) and any result compared. The stand-ins give
// back what the real callee would leave for the caller to read - screen
// points, depths, allocations, a moved packet pointer - and now and then
// disturb what the caller reads again after the call.
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <utility>

#include "bof3/symbols.gen.h"
#include "game/map_layers_callees.h"
#include "hook/detour.h"
#include "hook/log.h"

namespace map_layers {
namespace {

std::uint32_t Address(const void* p) { return static_cast<std::uint32_t>(reinterpret_cast<std::uintptr_t>(p)); }
unsigned char* At(std::uint32_t a) { return reinterpret_cast<unsigned char*>(static_cast<std::uintptr_t>(a)); }
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

constexpr unsigned short kGameControlWord = 0x027F;   // measured: psx_gte_float.cpp
unsigned short GetControlWord() {
    unsigned short cw;
    __asm__("fnstcw %0" : "=m"(cw));
    return cw;
}
void SetControlWord(unsigned short cw) { __asm__("fldcw %0" : : "m"(cw)); }

std::uint32_t g_rng;
std::uint32_t Next() {
    g_rng ^= g_rng << 13;
    g_rng ^= g_rng >> 17;
    g_rng ^= g_rng << 5;
    return g_rng;
}
bool OneIn(unsigned n) { return Next() % n == 0; }

// --- the stand-ins' log ----------------------------------------------------------

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

// --- AreaMap_Frame's --------------------------------------------------------------

// The shifts may move the scroll words and the redraw byte, which the caller
// adjusts in memory after them.
void DisturbScroll() {
    const std::uint32_t h = Hash();
    if (h % 4 == 0) MapView_ScrollX = static_cast<unsigned short>(static_cast<int>((h >> 8) % 0x640) - 0x320);
    if (h % 5 == 0) MapView_ElevationOffset = static_cast<unsigned short>(static_cast<int>((h >> 12) % 0x640) - 0x320);
    if (h % 7 == 0) MapView_Redraw = static_cast<unsigned char>(h >> 20);
}
void __cdecl StubFrameBd() { Record(1); }
void __cdecl StubColumnNext() { Record(2); DisturbScroll(); }
void __cdecl StubColumnPrev() { Record(3); DisturbScroll(); }
void __cdecl StubRowsNext() { Record(4); DisturbScroll(); }
void __cdecl StubRowsPrev() { Record(5); DisturbScroll(); }
void __cdecl StubPlaceRuns() {
    Record(6);
    const std::uint32_t h = Hash();
    if (h % 4 == 0) Camera_Angles[(h >> 8) % 3] = static_cast<short>(h >> 16);
}
short* __cdecl StubRotMatrix(const short* angles, short* m) {
    Record(7, Address(angles), Address(m), Sum(angles, 6));
    for (unsigned i = 0; i < 9; ++i) m[i] = static_cast<short>(Hash() >> i);
    const std::uint32_t h = Hash();
    if (h % 4 == 0) MapView_FocusX = static_cast<long>(h * 7);   // read again after this call
    if (h % 5 == 0) MapView_Elevation = static_cast<long>(h * 11);
    return m;
}
void __cdecl StubApplyMatrix(const short* m, const short* v, long* out) {
    Record(8, Address(m), Sum(m, 0x20), Pack(v));
    Record(8, static_cast<std::uint16_t>(v[2]));
    for (unsigned j = 0; j < 3; ++j) out[j] = static_cast<long>(Hash() * (j + 3));
    const std::uint32_t h = Hash();
    if (h % 4 == 0) Camera_ShiftX = static_cast<short>(h >> 8);
    if (h % 5 == 0) Camera_Distance = static_cast<short>(h >> 12);
}
void __cdecl StubSetRot(const unsigned long* m) { Record(9, Address(m), Sum(m, 0x20)); }
void __cdecl StubSetTrans(const unsigned long* m) {
    Record(10, Address(m), Sum(m, 0x20));
    if (Hash() % 4 == 0) MapView_Redraw = static_cast<unsigned char>(Hash() % 3);
}
void __cdecl StubBuild() {
    Record(11);
    if (Hash() % 3 == 0) MapView_Redraw = static_cast<unsigned char>(Hash() >> 8);
}

// --- MapView_Build's ------------------------------------------------------------------

constexpr unsigned kItems = 0x20;             // the draw items the fuzz uses: indices below this
constexpr unsigned kItemBytes = 0x1400;       // what the items' quads reach, buffer up to 7
constexpr unsigned kAreaBytes = 0x2000;

// A screen coordinate's bits: at each bound and a step past it, a NaN (quiet
// or signalling - written as bits, since a float return through the x87
// stack would quieten it), an infinity, or anywhere near the range.
std::uint32_t PickBits(float lo, float hi) {
    const std::uint32_t h = Hash();
    float f;
    switch (h % 10) {
        case 0: f = lo; break;
        case 1: f = hi; break;
        case 2: f = std::nextafter(lo, -1e30f); break;
        case 3: f = std::nextafter(hi, 1e30f); break;
        case 4: return 0x7F800000u | (h & 0x80000000u) | ((h >> 8) & 0x7FFFFFu) | 1u;
        case 5: f = (h & 0x100) ? INFINITY : -INFINITY; break;
        default: f = lo - 100.0f + static_cast<float>((h >> 8) % 1000000) * (hi - lo + 200.0f) / 1000000.0f; break;
    }
    std::uint32_t bits;
    std::memcpy(&bits, &f, 4);
    return bits;
}

void __cdecl StubLayersReset() {
    Record(20);
    for (unsigned k = 0; k < 8; ++k) DrawLayers[(Hash() + k * 97) % DrawLayers_count] = Hash() * (k + 1);
}
void __cdecl StubLoadVertex(const unsigned long* v) { Record(21, Address(v), v[0], v[1]); }
void __cdecl StubRtps() { Record(22); }
void __cdecl StubStoreXY(unsigned long* out) {
    Record(23, Address(out));
    out[0] = PickBits(-50.0f, 370.0f);
    Record(23);
    out[1] = PickBits(-200.0f, 290.0f);
}
unsigned __cdecl StubCellTextures(unsigned x, unsigned y, unsigned char* item, unsigned buffer) {
    Record(24, x, y, Address(item));
    Record(24, buffer);
    const std::uint32_t h = Hash();
    // The real one may release the +0x7E item and clear its word.
    if (h % 3 == 0) SetWord(item + 0x7E, 0);
    if (h % 5 == 0) SetWord(item + 0x8E, 0);
    if (h % 11 == 0) Scratch_Swap = (h >> 8) % 14;   // the inset is read again after every cell
    if (h % 13 == 0) return h;                        // only the low word is ORed in
    return h & 0xF000u;
}
unsigned short __cdecl StubAlloc() {
    Record(25);
    const std::uint32_t h = Hash();
    // The corner pointer is read again after an allocation.
    if (h % 9 == 0) MapView_CornerPtr = reinterpret_cast<unsigned char*>(&AreaMap_Corners) + (h >> 8) % 200 * 4;
    return static_cast<unsigned short>(h % 4 == 0 ? 0 : 1 + (h >> 4) % (kItems - 1));
}
void __cdecl StubLoadVertices3(const unsigned long* v) {
    Record(26, Address(v), v[0], v[1]);
    Record(26, v[2], v[3], v[4]);
    Record(26, v[5]);
}
void __cdecl StubRtpt() { Record(27); }
void __cdecl StubLink(unsigned long* tail, unsigned long item) { Record(28, Address(tail), item); }
void __cdecl StubStoreXY3(unsigned long* a, unsigned long* b, unsigned long* c) {
    Record(29, Address(a), Address(b), Address(c));
    for (unsigned long* out : {a, b, c}) {
        out[0] = Hash();
        Record(29);
        out[1] = Hash();
    }
}
void __cdecl StubDepths4(void* prim) {
    Record(30, Address(prim));
    for (unsigned k = 0; k < 4; ++k) SetDword(static_cast<unsigned char*>(prim) + 0x10 + 0x10 * k, Hash() + k);
}
void __cdecl StubDepthF3(float* a, float* b, float* c) {
    Record(31, Address(a), Address(b));   // c is a local of the caller's
    const std::uint32_t h = Hash();
    std::memcpy(a, &h, 4);
    const std::uint32_t h2 = h * 3, h3 = h * 5;
    std::memcpy(b, &h2, 4);
    std::memcpy(c, &h3, 4);
}
void __cdecl StubReleaseCell(unsigned char* cell) {
    Record(32, Address(cell), Word(cell + 2));
    if (Hash() % 2) SetWord(cell + 2, 0);
}
void __cdecl StubSort() { Record(33, DrawTable_Count, Sum(DrawTable, DrawTable_Count * 4u)); }

// --- MapView_CellTextures's, MapCell_DrawWalls's, AreaMap_ClutCycle's ---------------------

void __cdecl StubSetTexture(unsigned long texture, unsigned char* prim, int count) {
    Record(40, texture, Address(prim), static_cast<std::uint32_t>(count));
}
unsigned __cdecl StubRelease(unsigned short index) {
    Record(41, index);
    return Hash();
}

alignas(4) unsigned char g_packets[0x400];
std::uint32_t Packet(const void* p) { return Address(p) - Address(g_packets); }

void __cdecl StubSetPolyFT4(unsigned char* prim) { Record(50, Packet(prim)); }
void __cdecl StubSetShadeTex(unsigned char* prim, unsigned tge) { Record(51, Packet(prim), tge); }
// The vertices are the caller's locals: their three words are logged, not
// where they are; the fourth words are not read by the real one.
long __cdecl StubRotTransPers4(const short* v0, const short* v1, const short* v2, const short* v3, float* s0, float* s1,
                               float* s2, float* s3, long* p) {
    Record(52, Pack(v0), static_cast<std::uint16_t>(v0[2]), Pack(v1));
    Record(52, static_cast<std::uint16_t>(v1[2]), Pack(v2), static_cast<std::uint16_t>(v2[2]));
    Record(52, Pack(v3), static_cast<std::uint16_t>(v3[2]));
    Record(52, Packet(s0), Packet(s1), Packet(s2));
    Record(52, Packet(s3));
    *p = static_cast<long>(Hash());
    return static_cast<long>(Hash() * 3);
}
// Both arguments are bytes to the real one (draw_emit.cpp): the original
// pushes the slot with stale bits above.
void __cdecl StubCommit(unsigned slot, unsigned size) {
    Record(53, slot & 0xFFu, size & 0xFFu, Packet(Gfx_PacketNext));
    Gfx_PacketNext += size & 0xFFu;   // the caller's next primitive
}

unsigned char __cdecl StubTest(unsigned long code) {
    Record(60, static_cast<std::uint32_t>(code));
    const std::uint32_t h = Hash();
    return static_cast<unsigned char>(h % 3 == 0 ? 0 : h % 3 == 1 ? 1 : h >> 8);
}

const Callees kStubs = {
    StubFrameBd, StubColumnNext, StubColumnPrev, StubRowsNext, StubRowsPrev, StubPlaceRuns, StubRotMatrix,
    StubApplyMatrix, StubSetRot, StubSetTrans, StubBuild,
    StubLayersReset, StubLoadVertex, StubRtps, StubStoreXY, StubCellTextures, StubAlloc, StubLoadVertices3, StubRtpt,
    StubLink, StubStoreXY3, StubDepths4, StubDepthF3, StubReleaseCell, StubSort,
    StubSetTexture, StubRelease,
    StubSetPolyFT4, StubSetShadeTex, StubRotTransPers4, StubCommit,
    StubTest,
};

const void* StubFor(std::uint32_t target) {
    const auto f = [](auto p) { return reinterpret_cast<const void*>(p); };
    switch (target) {
        case 0x510630: return f(kStubs.frame_bd);
        case 0x56EA30: return f(kStubs.column_next);
        case 0x56E9A0: return f(kStubs.column_prev);
        case 0x56EB50: return f(kStubs.rows_next);
        case 0x56EAB0: return f(kStubs.rows_prev);
        case 0x571FF0: return f(kStubs.place_runs);
        case 0x5A8060: return f(kStubs.rot_matrix);
        case 0x5A7BF0: return f(kStubs.apply_matrix);
        case 0x5A8DE0: return f(kStubs.set_rot);
        case 0x5A8E00: return f(kStubs.set_trans);
        case 0x56EC00: return f(kStubs.build);
        case 0x56F5B0: return f(kStubs.layers_reset);
        case 0x5A8E30: return f(kStubs.load_vertex);
        case 0x5A8E90: return f(kStubs.rtps);
        case 0x5A90B0: return f(kStubs.store_xy);
        case 0x56F9B0: return f(kStubs.cell_textures);
        case 0x56FBD0: return f(kStubs.alloc);
        case 0x5A8E50: return f(kStubs.load_vertices3);
        case 0x5A8F60: return f(kStubs.rtpt);
        case 0x5A7560: return f(kStubs.link);
        case 0x5A90D0: return f(kStubs.store_xy3);
        case 0x5A9290: return f(kStubs.depths4);
        case 0x5A9130: return f(kStubs.depth_f3);
        case 0x56FC00: return f(kStubs.release_cell);
        case 0x56F5F0: return f(kStubs.sort);
        case 0x572A00: return f(kStubs.set_texture);
        case 0x56FC70: return f(kStubs.release);
        case 0x5A75D0: return f(kStubs.set_poly_ft4);
        case 0x5A77A0: return f(kStubs.set_shade_tex);
        case 0x5A85F0: return f(kStubs.rot_trans_pers4);
        case 0x461E50: return f(kStubs.commit);
        case 0x56FF00: return f(kStubs.test);
        default: bof3::Fatal("map_layers: no stand-in for a call to 0x%X", static_cast<unsigned>(target));
    }
}

// The copies and their calls, by capstone 2026-09-22 (every call in each
// function; no jump leaves any of them).
struct Call { std::uint32_t offset, target; };
constexpr Call kFrameCalls[] = {{0x14, 0x510630}, {0x162, 0x56EA30}, {0x183, 0x56E9A0}, {0x1A4, 0x56EB50},
                                {0x1C5, 0x56EAB0}, {0x1DE, 0x571FF0}, {0x22C, 0x5A8060}, {0x266, 0x5A7BF0},
                                {0x2AC, 0x5A8DE0}, {0x2B6, 0x5A8E00}, {0x2CB, 0x56EC00}};
constexpr Call kBuildCalls[] = {
    {0x7, 0x56F5B0},   {0x1DE, 0x5A8E30}, {0x1E3, 0x5A8E90}, {0x1ED, 0x5A90B0}, {0x290, 0x56F9B0}, {0x2A3, 0x56FBD0},
    {0x2F4, 0x56FBD0}, {0x340, 0x56FBD0}, {0x360, 0x56F9B0}, {0x436, 0x5A8E50}, {0x453, 0x5A8F60}, {0x495, 0x5A7560},
    {0x4E8, 0x5A7560}, {0x628, 0x5A7560}, {0x65F, 0x5A90D0}, {0x672, 0x5A9290}, {0x751, 0x5A8E50}, {0x782, 0x5A7560},
    {0x787, 0x5A8F60}, {0x7B9, 0x5A90D0}, {0x7CB, 0x5A9130}, {0x8B3, 0x5A8E50}, {0x8E5, 0x5A7560}, {0x8EA, 0x5A8F60},
    {0x91C, 0x5A90D0}, {0x92E, 0x5A9130}, {0x972, 0x56F5F0}, {0x980, 0x56FC00}, {0x98B, 0x56FC00}};
constexpr Call kTextureCalls[] = {{0x86, 0x572A00}, {0xB1, 0x572A00}, {0xEA, 0x572A00}, {0x106, 0x56FC70}};
constexpr Call kWallCalls[] = {{0x9D, 0x5A75D0},  {0xA5, 0x5A77A0},  {0x1D5, 0x5A85F0},
                               {0x1DB, 0x5A9290}, {0x1EA, 0x572A00}, {0x1F8, 0x461E50}};
constexpr Call kClutCalls[] = {{0x21, 0x56FF00}};

template <unsigned N>
void* Clone(const char* name, std::uint32_t base, std::uint32_t size, const Call (&calls)[N]) {
    bof3::CloneCall re_aimed[N];
    for (unsigned i = 0; i < N; ++i) re_aimed[i] = {calls[i].offset, StubFor(calls[i].target)};
    return bof3::CloneOriginal(name, base, size, re_aimed, static_cast<int>(N));
}

// --- the state and one round ----------------------------------------------------------

struct Region { unsigned char* at; unsigned size; };
template <class T> Region R(T& v) { return {reinterpret_cast<unsigned char*>(&v), sizeof v}; }
Region R(void* at, unsigned size) { return {static_cast<unsigned char*>(at), size}; }

constexpr unsigned kStateMax = 0x6000;
unsigned char g_saved[kStateMax], g_input[kStateMax], g_out[2][kStateMax];
Log g_logs[2];
std::uint32_t g_result;   // a function's result, as a region

unsigned Total(const Region* r, unsigned n) {
    unsigned total = 0;
    for (unsigned i = 0; i < n; ++i) total += r[i].size;
    if (total > kStateMax) bof3::Fatal("map_layers self-test: state of 0x%X bytes", total);
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

// Runs `theirs` then `ours` from the state as it stands; true if the state,
// the log or the result differ. The copy runs under the game's x87 control
// word.
template <class Theirs, class Ours>
bool Pair(const char* name, unsigned round, const Region* r, unsigned n, Theirs&& theirs, Ours&& ours, unsigned& bad) {
    const unsigned total = Total(r, n);
    Capture(r, n, g_input);
    g_seed = Next();
    const unsigned short saved_word = GetControlWord();
    for (int pass = 0; pass < 2; ++pass) {
        Apply(r, n, g_input);
        std::memset(&g_log, 0, sizeof g_log);
        if (pass == 0) {
            SetControlWord(kGameControlWord);
            theirs();
            SetControlWord(saved_word);
        } else {
            ours();
        }
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
            bof3::Log("shadow      map_layers %s MISMATCH: round %u, state region %u (0x%08X) +0x%X: %02X / %02X", name,
                      round, region, static_cast<unsigned>(Address(r[region].at)), at - base, g_out[0][at], g_out[1][at]);
        } else {
            unsigned at = 0;
            const unsigned kept = g_logs[0].n < kKeep ? g_logs[0].n : kKeep;
            while (at < kept && std::memcmp(g_logs[0].keep[at], g_logs[1].keep[at], 16) == 0) ++at;
            const std::uint32_t* a = at < kKeep ? g_logs[0].keep[at] : g_logs[0].keep[0];
            const std::uint32_t* b = at < kKeep ? g_logs[1].keep[at] : g_logs[1].keep[0];
            bof3::Log("shadow      map_layers %s MISMATCH: round %u, log of %u / %u calls, first difference at entry "
                      "%u: %X(%X, %X, %X) / %X(%X, %X, %X)", name, round, g_logs[0].n, g_logs[1].n, at, a[0], a[1],
                      a[2], a[3], b[0], b[1], b[2], b[3]);
        }
    }
    return true;
}

void Report(const char* name, unsigned rounds, unsigned bad, const char* detail) {
    bof3::Log("shadow      map_layers %s self-test: %u rounds (%s), %u MISMATCHES", name, rounds, detail, bad);
    if (bad) bof3::Fatal("%s differs from the original in %u of %u self-test rounds", name, bad, rounds);
}

// --- AreaMap_Frame ------------------------------------------------------------------------

void SelfTestFrame(void (__cdecl* theirs)()) {
    constexpr unsigned kRounds = 40000;
    const Region r[] = {
        R(&Game_AreaNumber, 4),               // and MoveScript_FAWord
        R(Camera_Matrix, 0x2A),               // to MapView_Redraw, past Field_Kind2Z / X
        R(Camera_Angles, 8),
        R(&Field_Kind2Hold, 0xE),             // to MapView_Elevation's end
        R(&MapView_ScrollX, 4),               // and MapView_ElevationOffset
        R(MoveScript_F3Divisor), R(AreaMap_Word1E), R(Camera_AnglesDrawn, 8), R(&Camera_ShiftX, 4),
        R(Camera_Distance),
    };
    constexpr unsigned n = sizeof r / sizeof r[0];
    Capture(r, n, g_saved);
    g_rng = 0x56E6C001u;
    unsigned bad = 0, area_bd = 0, moved_x = 0, moved_z = 0, held = 0, released = 0, shifts = 0, redrawn = 0,
             turned = 0;
    for (unsigned round = 0; round < kRounds; ++round) {
        Randomize(r, n);
        static const unsigned short kArea[] = {0xBD, 0xBC, 0xBE, 4};
        Game_AreaNumber = OneIn(4) ? kArea[Next() % 4] : static_cast<unsigned short>(Next() % 0x100);
        static const short kStep[] = {0, 1, 2, 3, 4, 0x10, 0x100, -1};
        const short step = OneIn(8) ? static_cast<short>(Next()) : kStep[Next() % 8];
        MoveScript_F3Divisor = step;
        // The focus a step or two from where the kind-2 object stands - or on it.
        auto near = [&](long kind2, std::uint32_t top) {
            const std::uint32_t on = top - ((static_cast<std::uint32_t>(kind2) & 0xFFFF8000u) >> 8);
            static const int kDelta[] = {0, 0, 1, -1, 2, -2};
            switch (Next() % 4) {
                case 0: return static_cast<long>(on);
                case 1: return static_cast<long>(on + static_cast<std::uint32_t>(step * kDelta[Next() % 6]));
                case 2: return static_cast<long>(on + static_cast<std::uint32_t>(kDelta[Next() % 6]));
                default: return static_cast<long>(Next());
            }
        };
        MapView_FocusX = near(Field_Kind2X, 0x7FFF);
        MapView_FocusZ = near(Field_Kind2Z, 0x8000);
        Field_Kind2Hold = static_cast<unsigned char>(Next() % 3 == 0 ? Next() : Next() % 2);
        static const int kScroll[] = {0x1FF, 0x200, 0x201, -0x1FF, -0x200, -0x201, 0};
        if (!OneIn(4)) {
            MapView_ScrollX = static_cast<unsigned short>(OneIn(2) ? kScroll[Next() % 7] : static_cast<int>(Next() % 0x440) - 0x220);
            MapView_ElevationOffset =
                static_cast<unsigned short>(OneIn(2) ? kScroll[Next() % 7] : static_cast<int>(Next() % 0x440) - 0x220);
        }
        if (OneIn(2)) MoveScript_FAWord = static_cast<unsigned short>(static_cast<int>(Next() % 9) - 4);
        if (OneIn(2)) AreaMap_Word1E = 0;
        MapView_Redraw = static_cast<unsigned char>(OneIn(4) ? Next() : Next() % 4);
        if (!OneIn(4)) {   // the angles as drawn, or one word off
            std::memcpy(Camera_AnglesDrawn, Camera_Angles, 8);
            if (OneIn(2)) Camera_AnglesDrawn[Next() % 4] ^= static_cast<unsigned short>(1u << (Next() % 16));
        }
        const bool bd = Game_AreaNumber == 0xBD;
        const std::uint32_t dx = ((0x7FFFu - static_cast<std::uint32_t>(MapView_FocusX)) << 8) -
                                 (static_cast<std::uint32_t>(Field_Kind2X) & 0xFFFF8000u);
        const std::uint32_t dz = ((0x8000u - static_cast<std::uint32_t>(MapView_FocusZ)) << 8) -
                                 (static_cast<std::uint32_t>(Field_Kind2Z) & 0xFFFF8000u);
        const bool hold_in = Field_Kind2Hold != 0;
        const bool angles_differ = std::memcmp(Camera_AnglesDrawn, Camera_Angles, 6) != 0;
        Pair("AreaMap_Frame", round, r, n, theirs, [] { AreaMap_Frame(); }, bad);
        if (bd) { ++area_bd; continue; }
        moved_x += dx != 0;
        moved_z += dz != 0;
        held += dx == 0 && dz == 0 && hold_in;
        constexpr unsigned hold_at = 4 + 0x2A + 8;   // Field_Kind2Hold opens region 3
        released += (dx != 0 || dz != 0 || hold_in) && g_out[0][hold_at] == 0;
        shifts += g_logs[0].counts[2] + g_logs[0].counts[3] + g_logs[0].counts[4] + g_logs[0].counts[5];
        redrawn += g_logs[0].counts[11];
        turned += angles_differ;
    }
    Apply(r, n, g_saved);
    char detail[320];
    std::snprintf(detail, sizeof detail,
                  "%u in area 0xBD; %u stepping x, %u z, %u held still, %u reaching the kind-2 object; %u shifts, %u "
                  "angle changes, %u rebuilds",
                  area_bd, moved_x, moved_z, held, released, shifts, turned, redrawn);
    Report("AreaMap_Frame", kRounds, bad, detail);
}

// --- MapView_Build ------------------------------------------------------------------------

void SelfTestBuild(void (__cdecl* theirs)()) {
    constexpr unsigned kRounds = 1500;
    const Region r[] = {
        R(DrawLayers, DrawLayers_count * 4), R(DrawTable, DrawTable_count * 4), R(DrawTable_Count), R(Scratch_Swap),
        R(MapView_Inset), R(MapView_Row), R(MapView_Column), R(Field_InputFlags), R(Cond_ByteFE), R(Cond_AngleFB),
        R(Camera_Distance), R(MapView_Origin, 4), R(&Field_Kind2Z, 8), R(MapView_BuildFlags), R(Gfx_BufferIndex),
        R(Draw_OtSlot), R(MapView_CellItems, MapView_CellItems_count), R(AreaMap_Header, 0x800),
        R(Prim_VertexScratch, 0x20), R(MapView_CornerPtr), R(MapView_ScreenXY, 8),
        R(At(bof3::addr::DamageScratch), 4), R(DrawItems, kItemBytes),
    };
    constexpr unsigned n = sizeof r / sizeof r[0];
    Capture(r, n, g_saved);
    g_rng = 0x56EC0001u;
    unsigned bad = 0, cells = 0, released = 0, items = 0, allocs = 0, sides = 0, links = 0, table = 0,
             no_columns = 0, fe = 0;
    for (unsigned round = 0; round < kRounds; ++round) {
        Randomize(r, n);
        Gfx_BufferIndex = static_cast<unsigned char>(OneIn(4) ? Next() % 8 : Next() % 2);
        MapView_Row = static_cast<short>(OneIn(4) ? 0x36 + Next() % 2 : Next() % 0x38);
        MapView_Column = static_cast<short>(OneIn(4) ? 0x1B : Next() % 0x1C);
        // The inset's divisions each at, and a step either side of, a multiple.
        const int off_axis = OneIn(4) ? static_cast<int>(Next() % 0x400)
                                      : 0x160 - 50 * static_cast<int>(Next() % 8) + static_cast<int>(Next() % 3) - 1;
        const int angle = 0x200 + (OneIn(2) ? off_axis : -off_axis);
        Cond_AngleFB = (Next() & 0xFFFF0000u) | static_cast<std::uint16_t>(angle);
        Camera_Distance = static_cast<short>(OneIn(4) ? static_cast<int>(Next())
                                                      : 650 * (static_cast<int>(Next() % 6) - 2) + static_cast<int>(Next() % 3) - 1);
        static const unsigned char kFE[] = {0x23, 0x23, 0x22, 0x24};
        if (OneIn(3)) Cond_ByteFE = kFE[Next() % 4];
        fe += Cond_ByteFE == 0x23;
        static const unsigned char kSlot[] = {4, 4, 6, 6, 5, 7};
        Draw_OtSlot = OneIn(4) ? static_cast<unsigned char>(Next()) : kSlot[Next() % 6];
        // The draw-table row threshold somewhere in the layers' range.
        const int threshold = static_cast<int>(Next() % 0x3C) - 2;
        MapView_Origin[0] = static_cast<short>(Next() % 64);
        MapView_Origin[1] = static_cast<short>(Next() % 64);
        const int kind2_x = static_cast<short>(Next() % 64);
        const int kind2_z = threshold + MapView_Origin[1] + MapView_Origin[0] - kind2_x - 8;
        Field_Kind2X = static_cast<long>((static_cast<std::uint32_t>(kind2_x) << 16) | (Next() & 0xFFFFu));
        Field_Kind2Z = static_cast<long>((static_cast<std::uint32_t>(kind2_z) << 16) | (Next() & 0xFFFFu));
        // A grid of up to 16 x 16 (the rows below it read too): corners with
        // equal and sign-edge bytes often, so the side items' comparisons tie.
        AreaMap_Header[0] = static_cast<unsigned char>(1 + Next() % 16);
        auto* corners = reinterpret_cast<unsigned char*>(&AreaMap_Corners);
        static const unsigned char kHeight[] = {0x7F, 0x80, 0x00, 0xFF, 0x01, 0x81};
        for (unsigned k = 0; k < 0x7C0; ++k)
            if (OneIn(2)) corners[k] = kHeight[Next() % 6];
        const unsigned empty = Next() % 4;   // how often a cell is off the map
        for (unsigned c = 0; c < MapView_CellItems_count / 4; ++c) {
            unsigned char* cell = MapView_CellItems + c * 4;
            cell[0] = static_cast<unsigned char>(Next() % (4 + empty * 4) == 0 ? 0 : 1 + Next() % 16);
            cell[1] = static_cast<unsigned char>(Next() % (4 + empty * 4) == 0 ? 0 : 1 + Next() % 15);
            SetWord(cell + 2, (Next() & 0xF000u) | (OneIn(3) ? 0 : 1 + Next() % (kItems - 1)));
        }
        for (unsigned i = 0; i < kItems; ++i) {
            unsigned char* item = DrawItems + i * 0x90;
            SetWord(item + 0x7E, OneIn(2) ? 0 : 1 + Next() % (kItems - 1));
            SetWord(item + 0x8E, OneIn(2) ? 0 : 1 + Next() % (kItems - 1));
            SetWord(item + 0x36, OneIn(2) ? 0 : OneIn(2) ? 1 + Next() % 3 : Next());
        }
        Pair("MapView_Build", round, r, n, theirs, [] { MapView_Build(); }, bad);
        const Log& l = g_logs[0];
        cells += l.counts[23] / 2;   // Gte_StoreScreenXY's stand-in logs two entries a call
        released += l.counts[32];
        items += l.counts[30];
        allocs += l.counts[25];
        sides += l.counts[31];
        links += l.counts[28];
        const unsigned count_at = DrawLayers_count * 4 + DrawTable_count * 4;
        table += g_out[0][count_at];
        const unsigned inset = static_cast<unsigned>(Dword(g_out[0] + count_at + 1));
        no_columns += static_cast<int>((0xEu - inset) << 1) <= 0;
    }
    Apply(r, n, g_saved);
    char detail[400];
    std::snprintf(detail, sizeof detail,
                  "%u cells to the cull test, %u released (off the map or culled), %u quads, %u allocations, %u side "
                  "triangles, %u list links, %u draw-table entries; %u rounds with Cond_ByteFE 0x23, %u with no "
                  "columns",
                  cells, released, items, allocs, sides, links, table, fe, no_columns);
    Report("MapView_Build", kRounds, bad, detail);
}

// --- MapView_CellTextures -----------------------------------------------------------------

void SelfTestCellTextures(unsigned (__cdecl* theirs)(unsigned, unsigned, unsigned char*, unsigned)) {
    constexpr unsigned kRounds = 60000;
    const Region r[] = {R(AreaMap_Header, kAreaBytes), R(DrawItems, kItemBytes), R(g_result)};
    constexpr unsigned n = sizeof r / sizeof r[0];
    Capture(r, n, g_saved);
    g_rng = 0x56F9B001u;
    unsigned bad = 0, none = 0, below = 0, next = 0, dropped = 0, flagged = 0;
    for (unsigned round = 0; round < kRounds; ++round) {
        Randomize(r, n);
        // An offset of at least 1 keeps the tile word off the header's own four bytes.
        const unsigned width = 1 + Next() % 16, height = 1 + Next() % 16, offset = 1 + Next() % 0x7F;
        AreaMap_Header[0] = static_cast<unsigned char>(width);
        AreaMap_Header[1] = static_cast<unsigned char>(height);
        SetWord(AreaMap_Header + 2, offset);
        const unsigned x = Next() % 17, y = Next() % 17;
        const unsigned tile = OneIn(5) ? 0 : 1 + Next() % 0x100;
        SetWord(AreaMap_Header + (x + width * y + offset * 2) * 2, tile);
        const unsigned run = (height * width + 1) / 2 + offset + tile;
        if (tile != 0 && run * 4 + 12 <= kAreaBytes) {
            if (OneIn(4)) SetDword(AreaMap_Header + run * 4 + 4, 0);
            if (OneIn(4)) SetDword(AreaMap_Header + run * 4 + 8, 0);
        }
        const unsigned index = Next() % kItems, buffer = OneIn(4) ? Next() % 8 : Next() % 2;
        unsigned char* item = DrawItems + index * 0x90;
        SetWord(item + 0x7E, OneIn(2) ? 0 : 1 + Next() % (kItems - 1));
        SetWord(item + 0x8E, OneIn(2) ? 0 : 1 + Next() % (kItems - 1));
        if (Word(AreaMap_Header + (x + width * y + offset * 2) * 2) == 0) ++none;
        Pair("MapView_CellTextures", round, r, n, [&] { g_result = theirs(x, y, item, buffer); },
             [&] { g_result = MapView_CellTextures(x, y, item, buffer); }, bad);
        const Log& l = g_logs[0];
        dropped += l.counts[41];
        below += Word(item + 0x8E) != 0 && l.counts[40] > 1;
        next += l.counts[40] == 3;
        flagged += Dword(g_out[0] + kAreaBytes + kItemBytes) != 0;
    }
    Apply(r, n, g_saved);
    char detail[256];
    std::snprintf(detail, sizeof detail,
                  "%u without a tile, %u with a +0x8E item, %u with all three textures, %u +0x7E items released; "
                  "%u non-zero results",
                  none, below, next, dropped, flagged);
    Report("MapView_CellTextures", kRounds, bad, detail);
}

// --- MapCell_DrawWalls --------------------------------------------------------------------

alignas(4) unsigned char g_record[4];

void SelfTestWalls(void (__cdecl* theirs)(const unsigned char*, unsigned, unsigned)) {
    constexpr unsigned kRounds = 60000;
    const Region r[] = {R(AreaMap_Header, 0x800), R(Gfx_PacketNext), R(Draw_OtSlot), R(g_packets), R(g_record)};
    constexpr unsigned n = sizeof r / sizeof r[0];
    Capture(r, n, g_saved);
    g_rng = 0x57150001u;
    unsigned bad = 0, per_kind[14] = {}, lifted = 0;
    for (unsigned round = 0; round < kRounds; ++round) {
        Randomize(r, n);
        const unsigned kind = 0x3F + round % 14;
        g_record[3] = static_cast<unsigned char>(kind);
        ++per_kind[kind - 0x3F];
        AreaMap_Header[0] = static_cast<unsigned char>(1 + Next() % 12);
        const unsigned b1 = Next() % 15, b0 = Next() % 15;
        auto* corners = reinterpret_cast<unsigned char*>(&AreaMap_Corners);
        static const unsigned char kHeight[] = {0x7F, 0x80, 0x00, 0xFF, 0x01, 0x81, 0x7E};
        for (unsigned k = 0; k < 0x7C0 - 0x30; ++k)
            if (OneIn(2)) corners[k] = kHeight[Next() % 7];
        Gfx_PacketNext = g_packets + 4 * (Next() % 0x40);
        const unsigned char* cell = corners + (AreaMap_Header[0] * b0 + b1) * 4;
        lifted += static_cast<signed char>(cell[-4 + 1]) > static_cast<signed char>(cell[0]);
        Pair("MapCell_DrawWalls", round, r, n, [&] { theirs(g_record, b1, b0); },
             [&] { MapCell_DrawWalls(g_record, b1, b0); }, bad);
    }
    Apply(r, n, g_saved);
    char detail[256];
    std::snprintf(detail, sizeof detail,
                  "%u per kind 0x3F..0x4C; %u with the left neighbour's first corner higher", per_kind[0], lifted);
    Report("MapCell_DrawWalls", kRounds, bad, detail);
}

// --- AreaMap_ClutCycle --------------------------------------------------------------------

alignas(4) unsigned char g_entry[0x100];

void SelfTestClutCycle(void (__cdecl* theirs)(const unsigned char*)) {
    constexpr unsigned kRounds = 60000;
    const Region r[] = {R(Gfx_ClutStrip, Gfx_ClutStrip_count * 2), R(Gfx_ClutStripDirty), R(Frame_Counter),
                        R(g_entry)};
    constexpr unsigned n = sizeof r / sizeof r[0];
    Capture(r, n, g_saved);
    g_rng = 0x571B4001u;
    unsigned bad = 0, copied = 0, refused = 0, passed = 0;
    for (unsigned round = 0; round < kRounds; ++round) {
        Randomize(r, n);
        static const unsigned kPeriod[] = {1, 2, 3, 0xFF, 0xFE};
        const unsigned period = OneIn(3) ? kPeriod[Next() % 5] : 1 + Next() % 0xFF;
        SetWord(g_entry, period);
        // Frames in order, each at or above the one before, then one of 0xFF.
        const unsigned count = Next() % 9;
        unsigned t = 1, frames[9];
        unsigned char* p = g_entry + 8;
        for (unsigned k = 0; k < count; ++k, p += 4) {
            t += OneIn(4) ? 0 : Next() % 40;
            if (t > 0xFE) t = 0xFE;
            frames[k] = t;
            SetDword(p, (t << 24) | (Next() % 0x200) << 12 | Next() % 0x200);
        }
        SetDword(p, 0xFF000000u | (Next() % 0x200) << 12 | Next() % 0x200);
        if (count != 0 && !OneIn(3)) Frame_Counter = frames[Next() % count] - 1 + period * (Next() % 0x10000);
        else if (OneIn(4)) Frame_Counter = 0xFEu + period * (Next() % 0x10000);
        Gfx_ClutStripDirty = static_cast<unsigned char>(OneIn(4) ? 1 : 0);
        const bool clean = Gfx_ClutStripDirty == 0;
        Pair("AreaMap_ClutCycle", round, r, n, [&] { theirs(g_entry); }, [&] { AreaMap_ClutCycle(g_entry); }, bad);
        const unsigned dirty_at = Gfx_ClutStrip_count * 2;
        passed += g_logs[0].counts[60];
        if (clean && g_out[0][dirty_at] == 1) ++copied;
        else if (clean) ++refused;
    }
    Apply(r, n, g_saved);
    char detail[200];
    std::snprintf(detail, sizeof detail, "%u copies from a clean strip, %u without; %u condition tests", copied, refused,
                  passed);
    Report("AreaMap_ClutCycle", kRounds, bad, detail);
}

// --- AreaMap_HeaderPass -------------------------------------------------------------------

// A stand-in per entry of AreaMap_EntryHandlers the mask reaches. One may
// lengthen its entry's step to 2, read after the call: the chain is laid out
// so that either step lands on an entry or on a zero.
template <unsigned K> void __cdecl EntryStandIn(const unsigned char* entry) {
    Record(70, K, Address(entry) - Address(AreaMap_Header), entry[2]);
    if (entry[2] == 1 && Hash() % 4 == 0) const_cast<unsigned char*>(entry)[2] = 2;
}
template <unsigned... K> void FillEntries(std::integer_sequence<unsigned, K...>) {
    ((AreaMap_EntryHandlers[K] = static_cast<unsigned long>(Address(reinterpret_cast<const void*>(&EntryStandIn<K>)))), ...);
}

void SelfTestHeaderPass(void (__cdecl* theirs)()) {
    constexpr unsigned kRounds = 30000;
    const Region r[] = {R(Draw_PassFlags), R(AreaMap_Header, kAreaBytes)};
    constexpr unsigned n = sizeof r / sizeof r[0];
    Capture(r, n, g_saved);
    static unsigned long saved_handlers[AreaMap_EntryHandlers_count];
    std::memcpy(saved_handlers, AreaMap_EntryHandlers, sizeof saved_handlers);
    FillEntries(std::make_integer_sequence<unsigned, AreaMap_EntryHandlers_count>());
    g_rng = 0x571AF001u;
    unsigned bad = 0, off = 0, empty = 0, calls = 0, high = 0;
    for (unsigned round = 0; round < kRounds; ++round) {
        Randomize(r, n);
        if (OneIn(4)) Draw_PassFlags &= 0xFB;
        else Draw_PassFlags |= 4;
        const unsigned base = 0x10 + Next() % 0x300;
        AreaMap_EntryBase = static_cast<unsigned short>(base);
        const unsigned count = OneIn(8) ? 0 : 1 + Next() % 10;
        unsigned char* p = AreaMap_Header + base * 4;
        for (unsigned k = 0; k < count; ++k, p += 4) {
            p[2] = 1;
            high += p[3] >= 0x80;
        }
        std::memset(p, 0, 12);
        if (!(Draw_PassFlags & 4)) ++off;
        else if (count == 0) ++empty;
        Pair("AreaMap_HeaderPass", round, r, n, theirs, [] { AreaMap_HeaderPass(); }, bad);
        calls += g_logs[0].counts[70];
    }
    std::memcpy(AreaMap_EntryHandlers, saved_handlers, sizeof saved_handlers);
    Apply(r, n, g_saved);
    char detail[200];
    std::snprintf(detail, sizeof detail, "%u with the pass off, %u empty; %u handler calls; %u kinds with bit 7 set",
                  off, empty, calls, high);
    Report("AreaMap_HeaderPass", kRounds, bad, detail);
}

}  // namespace

void SelfTest() {
    void* const frame = Clone("AreaMap_Frame", bof3::addr::AreaMap_Frame, 0x2DA, kFrameCalls);
    void* const build = Clone("MapView_Build", bof3::addr::MapView_Build, 0x9A1, kBuildCalls);
    void* const textures = Clone("MapView_CellTextures", bof3::addr::MapView_CellTextures, 0x11B, kTextureCalls);
    void* const walls = Clone("MapCell_DrawWalls", bof3::addr::MapCell_DrawWalls, 0x21F, kWallCalls);
    void* const clut = Clone("AreaMap_ClutCycle", bof3::addr::AreaMap_ClutCycle, 0x98, kClutCalls);
    void* const pass = bof3::CloneOriginal("AreaMap_HeaderPass", bof3::addr::AreaMap_HeaderPass, 0x46);
    g = kStubs;
    SelfTestFrame(reinterpret_cast<void(__cdecl*)()>(frame));
    SelfTestBuild(reinterpret_cast<void(__cdecl*)()>(build));
    SelfTestCellTextures(reinterpret_cast<unsigned(__cdecl*)(unsigned, unsigned, unsigned char*, unsigned)>(textures));
    SelfTestWalls(reinterpret_cast<void(__cdecl*)(const unsigned char*, unsigned, unsigned)>(walls));
    SelfTestClutCycle(reinterpret_cast<void(__cdecl*)(const unsigned char*)>(clut));
    SelfTestHeaderPass(reinterpret_cast<void(__cdecl*)()>(pass));
    g = kOriginals;
}

}  // namespace map_layers
