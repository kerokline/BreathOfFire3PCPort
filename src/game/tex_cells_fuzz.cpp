// BOF3X_SHADOW=tex_cells: a differential fuzz of the glyph and cell texture
// builders, their texel unpackers and helpers against byte-copies of Capcom's,
// once at start-up, on the fake DirectDraw of ddraw_fuzz.h and a fake device of
// this file's own. docs/tex-cells.md section 6.
//
// The copies: the glyph builder with Dd_ClearSurface after it as one block
// (0x5A2CA0..0x5A2EAC; its call of the clear stays inside), D3d_FitTextureSize
// alone, the two cell builders with D3d_FreeCellTexture between them as one
// block (0x5A32B0..0x5A3A5A; the build's call of the free stays inside), and
// the five unpackers as one block (0x5A9E1E..0x5AA5D6, no calls). The builders'
// calls are re-aimed: the unpackers at recorders that log the call and run the
// unpacker block's copy, Gfx_ClutPixels at a stand-in, the clear and the fit
// at their copies; the surface helpers and the CRT's _ftol 0x5B9550 (plain x87,
// no CRT state) go where the original called. Ours runs on recorders that log
// alike and run ours, through tex_cells::g, and calls our helpers by name. So a
// builder round compares Capcom's whole tree against ours.
//
// Compared each round: the log (every COM call with its arguments, the device's
// EndScene and BeginScene, every unpacker call with where it writes, reads and
// looks up, every CLUT lookup), the descriptor each CreateSurface was handed,
// every byte of every fake surface's buffer, every byte of state either side
// could touch (Font_TexCache and its overrun into the vertex block,
// D3d_CellTexCache with the entries past it and D3d_DeviceDesc,
// SpriteCell_Table and the dwords before it, Gfx_ClutRows, Gfx_RenderFlags,
// the pixel formats, the DirectDraw globals and Font_GlyphData), and the
// return. The fakes and the stand-ins disturb memory now and then after a call
// so every read the builders make is ordered against every call.
#include <windows.h>

#include <cstdint>
#include <cstdio>
#include <cstring>

#include "bof3/symbols.gen.h"
#include "game/ddraw_fuzz.h"
#include "game/tex_cells_callees.h"
#include "hook/detour.h"
#include "hook/log.h"

namespace tex_cells {
namespace {

using ddraw_fuzz::Record;

unsigned char* At(U address) { return reinterpret_cast<unsigned char*>(static_cast<std::uintptr_t>(address)); }
U Addr(const void* p) { return static_cast<U>(reinterpret_cast<std::uintptr_t>(p)); }
U Long(U address) {
    U v;
    std::memcpy(&v, At(address), sizeof v);
    return v;
}
void PutLong(U address, U v) { std::memcpy(At(address), &v, sizeof v); }
void PutWord(U address, U v) {
    const std::uint16_t w = static_cast<std::uint16_t>(v);
    std::memcpy(At(address), &w, sizeof w);
}

// --- random numbers of our own (the game's CRT is not up) -------------------------

U g_rand = 0x5A32B0u;
U Next() {
    U x = g_rand;
    x ^= x << 13;
    x ^= x >> 17;
    x ^= x << 5;
    return g_rand = x;
}
template <unsigned N> U Pick(const U (&values)[N]) { return values[Next() % N]; }
U Range(std::int32_t lo, std::int32_t hi) {   // lo..hi inclusive
    return static_cast<U>(lo + static_cast<std::int32_t>(Next() % static_cast<U>(hi - lo + 1)));
}

// --- the state -------------------------------------------------------------------

struct Region {
    U address, bytes;
};
const Region kRegions[] = {
    {kFontCache, 0xA30},       // Font_TexCache, and entry 128 over the vertex block (D18)
    {kCellCache, 0x1528},      // D3d_CellTexCache, entries 128.. over D3d_DeviceDesc, and 0x7CC334..0x7CC360
    {0x6BE9F8, 0x20},          // the surface pointers before SpriteCell_Table, Dd_CellBackdrop among them
    {kCellTable, 0x4000},      // SpriteCell_Table
    {kClutRows, 0x1000},       // Gfx_ClutRows
    {kRenderFlags, 4},         // Gfx_RenderFlags
    {kPixelFormat, 0x40},      // Gfx_PixelFormat and format record 0's DDPIXELFORMAT at 0x7DED80
};
constexpr U kStateBytes = 0xA30 + 0x1528 + 0x20 + 0x4000 + 0x1000 + 4 + 0x40;
struct State {
    unsigned char bytes[kStateBytes];
};
State g_saved, g_start, g_theirs_state, g_ours_state;

void Capture(State& s) {
    U at = 0;
    for (const Region& r : kRegions) {
        std::memcpy(s.bytes + at, At(r.address), r.bytes);
        at += r.bytes;
    }
}
void Restore(const State& s) {
    U at = 0;
    for (const Region& r : kRegions) {
        std::memcpy(At(r.address), s.bytes + at, r.bytes);
        at += r.bytes;
    }
}
bool SameState(const State& a, const State& b, U* where) {
    U at = 0;
    for (const Region& r : kRegions) {
        for (U i = 0; i < r.bytes; ++i)
            if (a.bytes[at + i] != b.bytes[at + i]) {
                *where = r.address + i;
                return false;
            }
        at += r.bytes;
    }
    return true;
}

// The VRAM shadow, randomised once for the whole fuzz and put back after.
constexpr U kVramBytes = 0x100000;
unsigned char g_vram_saved[kVramBytes];

// The converted palettes the Gfx_ClutPixels stand-in hands out: 64 of 32
// bytes, each read up to 256 entries of 4 bytes on. A third of the entries 0
// (transparent to the cell unpackers), some 0 only in their low or high half
// (16-bit against 32-bit tests).
constexpr U kPaletteBytes = 64 * 32 + 1024;
alignas(4) unsigned char g_palettes[kPaletteBytes];
void RandomPalettes() {
    for (U i = 0; i < kPaletteBytes; i += 4) {
        U v = Next();
        switch (Next() % 7) {
        case 0:
        case 1: v = 0; break;
        case 2: v &= 0xFFFF0000u; break;
        case 3: v &= 0xFFFFu; break;
        default: break;
        }
        PutLong(Addr(g_palettes + i), v);
    }
}

// Two glyph tables of kGlyphs glyphs, one after the other; Font_GlyphData
// points at one or the other. Glyph indices stay inside a table: past its end
// the original reads whatever is there.
constexpr U kGlyphs = 64;
constexpr U kGlyphBytes = 0x120;
unsigned char* g_glyphs;   // 2 * kGlyphs * kGlyphBytes

// --- the round and the disturbances -------------------------------------------------

enum Kind { kGlyphRound, kBuildRound, kRefreshRound, kOtherRound };
Kind g_kind = kOtherRound;
U g_round;
U g_entry;        // the round's cache entry
U g_first, g_count;
U g_generation;   // the Gfx_ClutRows dword the round's CLUT names
void* g_stage[2];
void* g_entry_surfaces[2];   // surfaces that may sit in the entry
void* g_backdrop;

U Mix(U salt) {
    U h = (g_round * 0x9E3779B1u) ^ (salt * 0x85EBCA6Bu) ^ (ddraw_fuzz::g_log->n * 0xC2B2AE35u);
    h ^= h >> 16;
    h *= 0x7FEB352Du;
    h ^= h >> 15;
    h *= 0x846CA68Bu;
    h ^= h >> 16;
    return h;
}

// A cell record the unpackers can take: w and h 8..120 (a size nibble of 0
// runs the original away), x from -160 so the texels start inside the stage
// row, y inside the stage's 256 rows unless `spill`, u, v and the tpage
// anything.
void RandomRecord(U address, U h, bool spill) {
    const U w_nib = (h & 3) == 0 ? ((h >> 2) & 1 ? 15u : 1u) : 1 + (h >> 3) % 15;
    const U h_nib = (h & 0x30) == 0 ? ((h >> 6) & 1 ? 15u : 1u) : 1 + (h >> 7) % 15;
    const std::int32_t w = static_cast<std::int32_t>(w_nib * 8), hh = static_cast<std::int32_t>(h_nib * 8);
    const U x = spill ? Range(-160, 300) : Range(-160, 160 - w);
    const U y = spill ? Range(-128, 127) : Range(-128, 128 - hh);
    PutWord(address, x);
    At(address)[2] = static_cast<unsigned char>(y);
    At(address)[3] = static_cast<unsigned char>(h_nib << 4 | w_nib);
    PutWord(address + 4, Next());
    At(address)[6] = static_cast<unsigned char>(Next());
    At(address)[7] = static_cast<unsigned char>(Next());
}

void RandomCaps(U h) {
    const U mins[] = {0, 1, 2, 3, 8, 16, 0, 1};
    const U maxes[] = {0x100, 0x800, 0x40, 0x20, 0x10, 0x30, 0x64, 0xFFFFFFFFu, 0x400, 0x8};
    At(kTexCaps)[0] = static_cast<unsigned char>(h % 4 == 0 ? Next() : (h >> 2) % 4 == 0 ? 0 : ((h >> 4) & 0x22));
    PutLong(kMinTexWidth, Pick(mins));
    PutLong(kMinTexHeight, Pick(mins));
    PutLong(kMaxTexWidth, Next() % 3 ? Pick(maxes) : 1 + Next() % 400);
    PutLong(kMaxTexHeight, Next() % 3 ? Pick(maxes) : 1 + Next() % 400);
}

// What a callee may do to memory before the builder's next read, always to
// values the original can take: the render flags' two bits, the staging
// surface, the depth byte, the round's CLUT row generation; for the glyph, the
// glyph table and the entry's surface and texture; for the cells, a record,
// the entry's surface, texture, used size and extent, the device's caps.
void DisturbWith(U what, U h);
void Disturb(U what) {
    const U h = Mix(what);
    if (h % 4) return;
    // Both passes must be disturbed alike: what the disturbance draws comes
    // from a generator seeded by the hash, not from the round's.
    const U saved = g_rand;
    g_rand = h | 1;
    DisturbWith(what, h);
    g_rand = saved;
}
void DisturbWith(U what, U h) {
    const U v = h >> 8;
    switch ((h >> 4) % 12) {
    case 0: At(kRenderFlags)[0] ^= 1; return;
    case 1: At(kRenderFlags)[0] ^= 0x20; return;
    case 2: PutLong(kStage, Addr(g_stage[v % 2])); return;
    case 3: At(kPixelFormat)[3] = v % 3 ? 2 : 4; return;
    case 4: PutLong(g_generation, v * 0x10001u); return;
    default: break;
    }
    if (g_kind == kGlyphRound) {
        switch ((h >> 4) % 12) {
        case 5: PutLong(kGlyphData, Addr(g_glyphs + (v % 2) * kGlyphs * kGlyphBytes)); return;
        case 6:
        case 7:
            // The entry's surface: 0 only before the builder tests it (the
            // Gfx_ClutPixels call comes first) - after, the original uses it.
            PutLong(g_entry + 8, what == 0x20 && v % 3 == 0 ? 0 : Addr(g_entry_surfaces[v % 2]));
            return;
        case 8: PutLong(g_entry + 0xC, v); return;
        default: PutWord(g_entry, v); return;
        }
    }
    if (g_kind == kBuildRound || g_kind == kRefreshRound) {
        switch ((h >> 4) % 12) {
        case 5:
        case 6:
            if (g_count && g_count <= 0x800) RandomRecord(kCellTable + ((g_first + v % g_count) & 0x7FF) * 8, v, false);
            return;
        case 7: PutLong(g_entry + 0x20, Addr(g_entry_surfaces[v % 2])); return;
        case 8: PutLong(g_entry + 0x24, v % 2 ? Addr(ddraw_fuzz::TextureOf(g_entry_surfaces[v % 2])) : 0); return;
        case 9:
            PutWord(g_entry + 0, v % 330);
            PutWord(g_entry + 2, (v >> 12) % 270);
            return;
        case 10:
            PutWord(g_entry + 8, Range(-160, 0));
            PutWord(g_entry + 0xA, Range(0, 160));
            PutWord(g_entry + 0xC, Range(-128, 0));
            PutWord(g_entry + 0xE, Range(0, 128));
            return;
        default: RandomCaps(v); return;
        }
    }
}

void* __cdecl ClutStandIn(unsigned clut) {
    // Gfx_ClutPixels masks its argument to 16 bits first (0x5A04C5).
    Record(0x20, clut & 0xFFFF);
    Disturb(0x20);
    return g_palettes + ((clut & 0xFFFF) * 7 % 64) * 32;
}

// --- the fake device: BeginScene and EndScene --------------------------------------

constexpr U kBeginSceneCall = 0x30, kEndSceneCall = 0x31;
struct Device {
    void** vtbl;
};
void* g_device_vtbl[64];
Device g_device = {g_device_vtbl};
long __stdcall DeviceBeginScene(Device*) {
    Record(kBeginSceneCall);
    Disturb(kBeginSceneCall);
    return 0;
}
// The scaled height's precision. The build's EndScene comes just before it
// reads the used size and fits the texture, so a third of the time the fake
// sets a used size and a maximum texture width from triples (texture width,
// used width, used height) whose `fild / fidiv / fimul` truncates differently
// at 24 or 64 bits of precision than in doubles - about one triple in 2,400 in
// the ranges the cells give, which random extents almost never hit (found by
// exact arithmetic in the scratchpad, 2026-09-23).
U g_control_word;
struct Triple {
    std::uint16_t texture_w, used_w, used_h;
};
const Triple kAt24[] = {{13, 22, 22}, {15, 22, 44}, {5, 23, 69},  {7, 23, 23},  {14, 23, 46}, {20, 23, 69},
                        {21, 25, 75}, {15, 26, 78}, {5, 27, 81},  {17, 28, 84}, {9, 29, 87},  {15, 29, 58},
                        {21, 30, 90}, {17, 31, 93}, {9, 33, 99},  {18, 35, 105}, {21, 36, 108}, {3, 37, 37}};
const Triple kAt64[] = {{5, 7, 21},   {5, 14, 42},  {10, 14, 42}, {15, 21, 21}, {15, 21, 42}, {13, 22, 22},
                        {15, 22, 44}, {21, 25, 75}, {15, 26, 26}, {17, 26, 78}, {21, 27, 81}, {5, 28, 84},
                        {20, 28, 84}, {15, 29, 87}, {21, 30, 90}, {17, 31, 93}, {9, 33, 99},  {19, 35, 105},
                        {19, 36, 108}, {5, 37, 37}, {10, 37, 111}};
unsigned g_precision_seeded;
void SeedPrecision() {
    const U h = Mix(0x55);
    if (h % 3) return;
    const bool at24 = g_control_word == 0x007F;
    const Triple& t = at24 ? kAt24[(h >> 8) % (sizeof kAt24 / sizeof kAt24[0])]
                           : kAt64[(h >> 8) % (sizeof kAt64 / sizeof kAt64[0])];
    PutWord(g_entry + 0, t.used_w);
    PutWord(g_entry + 2, t.used_h);
    At(kTexCaps)[0] = 0;
    PutLong(kMaxTexWidth, t.texture_w);
    PutLong(kMaxTexHeight, 0x800);
    ++g_precision_seeded;
}

long __stdcall DeviceEndScene(Device*) {
    Record(kEndSceneCall);
    if (g_kind == kBuildRound) SeedPrecision();
    Disturb(kEndSceneCall);
    return 0;
}
long __stdcall DeviceTrap(void*) {
    bof3::Fatal("tex_cells: a fake device method other than BeginScene or EndScene was called");
}
void BuildDevice() {
    for (void*& slot : g_device_vtbl) slot = reinterpret_cast<void*>(&DeviceTrap);
    g_device_vtbl[9] = reinterpret_cast<void*>(&DeviceBeginScene);    // +0x24
    g_device_vtbl[10] = reinterpret_cast<void*>(&DeviceEndScene);     // +0x28
}

// --- the recorders ------------------------------------------------------------------

ddraw_fuzz::Log g_theirs, g_ours;

[[noreturn]] void Refuse() { bof3::Fatal("the glyph and cell textures differ from the original (stopped at the first mismatch)"); }

// True if our pass's last recorded call is the one Capcom's copy made at the
// same place. Ours runs an unpacker only then: one handed the wrong source or
// destination would read or write where the original never does.
bool SameAsTheirs() {
    const unsigned i = g_ours.n - 1;
    if (i >= ddraw_fuzz::kMaxCalls || i >= g_theirs.n) return false;
    const ddraw_fuzz::Call& a = g_ours.calls[i];
    const ddraw_fuzz::Call& b = g_theirs.calls[i];
    return a.what == b.what && std::memcmp(a.a, b.a, sizeof a.a) == 0;
}

U g_src_base;   // the VRAM shadow for the cells, the glyph tables for the glyphs

void RecordUnpack(U what, const void* dst, const void* src, const void* palette, int w, int h, int pitch) {
    U surface = 0xFFFFFFFFu, offset = Addr(dst);
    ddraw_fuzz::Locate(dst, &surface, &offset);
    Record(what, surface, offset, Addr(src) - g_src_base, Addr(palette) - Addr(g_palettes), static_cast<U>(w),
           static_cast<U>(h), static_cast<U>(pitch));
}

using GlyphFn = void(__cdecl*)(void*, const void*, const void*, int);
GlyphFn g_their_glyph;
Unpack g_their_cell[4];   // 4, 8, 4 flipped, 8 flipped
const Unpack kOurCell[4] = {&::Cell_Unpack4, &::Cell_Unpack8, &::Cell_Unpack4Flip, &::Cell_Unpack8Flip};

template <int Ours> void __cdecl RecordGlyph(void* d, const void* s, const void* p, int pitch) {
    RecordUnpack(0x10, d, s, p, 0, 0, pitch);
    if (!Ours || SameAsTheirs()) (Ours ? &::Font_UnpackGlyph : g_their_glyph)(d, s, p, pitch);
    Disturb(0x10);
}
template <int Which, int Ours> void __cdecl RecordCell(void* d, const void* s, const void* p, int w, int h, int pitch) {
    RecordUnpack(0x11 + Which, d, s, p, w, h, pitch);
    if (!Ours || SameAsTheirs()) (Ours ? kOurCell[Which] : g_their_cell[Which])(d, s, p, w, h, pitch);
    Disturb(0x11 + Which);
}

const Callees kOursRecorded = {RecordGlyph<1>, RecordCell<0, 1>, RecordCell<1, 1>, RecordCell<2, 1>, RecordCell<3, 1>,
                               ClutStandIn};

// --- the copies ------------------------------------------------------------------------

constexpr U kGlyphBlock = 0x5A2CA0, kGlyphBlockBytes = 0x20C;   // Font_BuildGlyphTexture, Dd_ClearSurface; ret at 0x5A2EAB
constexpr U kFit = 0x59F9B0, kFitBytes = 0x9A;                   // D3d_FitTextureSize; ret at 0x59FA49
constexpr U kCellBlock = 0x5A32B0, kCellBlockBytes = 0x7AA;      // build, free, refresh; ret at 0x5A3A59
constexpr U kUnpackers = 0x5A9E1E, kUnpackersBytes = 0x7B8;      // the five unpackers; ret at 0x5AA5D5

using GlyphBuildFn = void(__cdecl*)(int, unsigned, unsigned);
using ClearFn = long(__cdecl*)(void*);
using FitFn = void(__cdecl*)(unsigned, unsigned, unsigned*, unsigned*);
using BuildFn = void(__cdecl*)(int, unsigned, unsigned, unsigned, unsigned);
using FreeFn = void(__cdecl*)(int);
using RefreshFn = void(__cdecl*)(int, unsigned, unsigned, unsigned);

struct Clones {
    GlyphBuildFn glyph;
    ClearFn clear;
    FitFn fit;
    BuildFn build;
    FreeFn free;
    RefreshFn refresh;
} g_clone;

void MakeClones() {
    auto* unpackers = static_cast<unsigned char*>(bof3::CloneOriginal("Font_UnpackGlyph", kUnpackers, kUnpackersBytes));
    auto* fit = static_cast<unsigned char*>(bof3::CloneOriginal("D3d_FitTextureSize", kFit, kFitBytes));
    if (!unpackers || !fit) bof3::Fatal("tex_cells: CloneOriginal returned null");
    g_their_glyph = reinterpret_cast<GlyphFn>(unpackers);
    g_their_cell[0] = reinterpret_cast<Unpack>(unpackers + 0x220);   // 0x5AA03E
    g_their_cell[1] = reinterpret_cast<Unpack>(unpackers + 0x3C2);   // 0x5AA1E0
    g_their_cell[2] = reinterpret_cast<Unpack>(unpackers + 0x4D8);   // 0x5AA2F6
    g_their_cell[3] = reinterpret_cast<Unpack>(unpackers + 0x690);   // 0x5AA4AE
    g_clone.fit = reinterpret_cast<FitFn>(fit);

    const void* clut = reinterpret_cast<const void*>(&ClutStandIn);
    const void* g0 = reinterpret_cast<const void*>(&RecordGlyph<0>);
    const bof3::CloneCall glyph[] = {
        {0x029, clut, 0x5A04C0},    {0x04C, nullptr, 0x59F840}, {0x080, nullptr, 0x59F860},
        {0x0CA, g0, 0x5A9E1E},      {0x103, nullptr, 0x59F900}, {0x15B, g0, 0x5A9E1E},
    };
    auto* glyph_block = static_cast<unsigned char*>(bof3::CloneOriginal(
        "Font_BuildGlyphTexture", kGlyphBlock, kGlyphBlockBytes, glyph, sizeof glyph / sizeof glyph[0]));
    if (!glyph_block) bof3::Fatal("tex_cells: CloneOriginal returned null");
    g_clone.glyph = reinterpret_cast<GlyphBuildFn>(glyph_block);
    g_clone.clear = reinterpret_cast<ClearFn>(glyph_block + 0x1D0);   // 0x5A2E70

    const void* clear = glyph_block + 0x1D0;
    const void* c4 = reinterpret_cast<const void*>(&RecordCell<0, 0>);
    const void* c8 = reinterpret_cast<const void*>(&RecordCell<1, 0>);
    const void* c4f = reinterpret_cast<const void*>(&RecordCell<2, 0>);
    const void* c8f = reinterpret_cast<const void*>(&RecordCell<3, 0>);
    const bof3::CloneCall cells[] = {
        // D3d_BuildCellTexture
        {0x084, clear, 0x5A2E70},   {0x0A1, nullptr, 0x59F840}, {0x0CD, clut, 0x5A04C0},
        {0x1CA, c8f, 0x5AA4AE},     {0x1D1, c4f, 0x5AA2F6},     {0x1F0, c8, 0x5AA1E0},
        {0x1F7, c4, 0x5AA03E},      {0x375, nullptr, 0x59F860}, {0x407, fit, 0x59F9B0},
        {0x425, nullptr, 0x59F900}, {0x463, nullptr, 0x5B9550},
        // D3d_RefreshCellTexture
        {0x52E, clear, 0x5A2E70},   {0x54A, nullptr, 0x59F840}, {0x577, clut, 0x5A04C0},
        {0x653, c8f, 0x5AA4AE},     {0x65A, c4f, 0x5AA2F6},     {0x679, c8, 0x5AA1E0},
        {0x680, c4, 0x5AA03E},
    };
    auto* cell_block = static_cast<unsigned char*>(bof3::CloneOriginal(
        "D3d_BuildCellTexture", kCellBlock, kCellBlockBytes, cells, sizeof cells / sizeof cells[0]));
    if (!cell_block) bof3::Fatal("tex_cells: CloneOriginal returned null");
    g_clone.build = reinterpret_cast<BuildFn>(cell_block);
    g_clone.free = reinterpret_cast<FreeFn>(cell_block + 0x4E0);      // 0x5A3790
    g_clone.refresh = reinterpret_cast<RefreshFn>(cell_block + 0x520);   // 0x5A37D0
}

// --- random state ---------------------------------------------------------------------------

const U kBytesPerPixel[] = {2, 4, 2, 4, 2, 4, 3, 0};

// The depth byte 0x7DED63 (2 or 4 mostly) and format record 0's DDPIXELFORMAT
// at 0x7DED80 (the one every create here names), its bit count the same depth.
void RandomFormats() {
    for (U i = 0; i < 0x40; i += 4) PutLong(kPixelFormat + i, Next());
    At(kPixelFormat)[3] = static_cast<unsigned char>(Next() % 10 ? Pick(kBytesPerPixel) : Next());
    PutLong(kPixelFormat + 0x20, 0x20);
    PutLong(kPixelFormat + 0x2C, At(kPixelFormat)[3] == 4 ? 32 : 16);
}
U BytesPerPixel() { return At(kPixelFormat)[3] == 4 ? 4 : 2; }

void RandomBytes(U address, U bytes) {
    for (U i = 0; i < bytes; i += 4) {
        const U v = Next();
        std::memcpy(At(address + i), &v, bytes - i < 4 ? bytes - i : 4);
    }
}

// Sets the x87 control word for a round and puts the old one back after: the
// cell build's scaled height rounds at its precision.
struct ControlWord {
    unsigned short saved;
    explicit ControlWord(unsigned short cw) {
        __asm__ volatile("fnstcw %0" : "=m"(saved));
        __asm__ volatile("fldcw %0" : : "m"(cw));
    }
    ~ControlWord() { __asm__ volatile("fldcw %0" : : "m"(saved)); }
};
const U kControlWords[] = {0x027F, 0x007F, 0x037F};

bool Compare(const char* name, unsigned r, U ret_ours, U ret_theirs, unsigned* bad) {
    char why[200] = "";
    U where = 0;
    bool same = ddraw_fuzz::SameLog(g_ours, g_theirs, why);
    if (same && !ddraw_fuzz::SamePixels(why)) same = false;
    if (same && !SameState(g_ours_state, g_theirs_state, &where)) {
        same = false;
        std::snprintf(why, sizeof why, "memory at 0x%X", (unsigned)where);
    }
    if (same && ret_ours != ret_theirs) {
        same = false;
        std::snprintf(why, sizeof why, "returned 0x%X, the original 0x%X", (unsigned)ret_ours, (unsigned)ret_theirs);
    }
    if (!same) {
        if (*bad < 4) bof3::Log("shadow      tex_cells MISMATCH: %s round %u: %s", name, r, why);
        ++*bad;
        Refuse();
    }
    return same;
}

// The two passes of one round: Capcom's copy on its log, then ours on ours
// through the recorders, from the same memory. Returns through the outs.
template <typename Theirs, typename Ours> void TwoPasses(Theirs theirs, Ours ours, U* ret_theirs, U* ret_ours) {
    Capture(g_start);
    ddraw_fuzz::BeginPass(0);
    g_theirs.Clear();
    ddraw_fuzz::g_log = &g_theirs;
    *ret_theirs = theirs();
    Capture(g_theirs_state);

    Restore(g_start);
    ddraw_fuzz::BeginPass(1);
    g_ours.Clear();
    ddraw_fuzz::g_log = &g_ours;
    g = kOursRecorded;
    *ret_ours = ours();
    g = kOriginals;
    Capture(g_ours_state);
    ddraw_fuzz::g_log = nullptr;
}

// Whether the pass just run ended at a failed call of `what` (the last one).
bool EndedAt(const ddraw_fuzz::Log& log, U what) {
    return log.n > 0 && log.n <= ddraw_fuzz::kMaxCalls && log.calls[log.n - 1].what == what;
}

// --- the unpackers, directly --------------------------------------------------------------------

constexpr U kSourceBytes = 0x800 * 272;
constexpr U kDstBytes = 0x110000;
unsigned char* g_source;
unsigned char* g_dst[2];
const std::int32_t kPitches[] = {48, 64, 96, 128, 100, 130, 640, 641, 1280, 1283, 2048, 4, 0, -64, -640, -1283};

struct UnpackCoverage {
    unsigned glyph, glyph_wide, cell[4], cell_wide, flipped_texels, transparent;
};

void UnpackRounds(unsigned rounds, unsigned* bad, UnpackCoverage* c) {
    const U w4[] = {8, 16, 24, 32, 64, 120, 128, 248};
    const U w8[] = {4, 8, 12, 16, 24, 60, 120, 124, 252};
    const U hs[] = {1, 2, 7, 8, 16, 24, 120, 128};
    for (unsigned r = 0; r < rounds; ++r) {
        RandomFormats();
        RandomPalettes();
        for (int i = 0; i < 64; ++i) {
            const U v = Next();
            std::memcpy(g_source + Next() % (kSourceBytes - 4), &v, 4);
        }
        const int which = static_cast<int>(r % 5);   // 0 the glyph, 1..4 the cells
        const U w = which == 0 ? 24 : (which == 1 || which == 3) ? Pick(w4) : Pick(w8);
        const U h = which == 0 ? 24 : Pick(hs);
        std::int32_t pitch = kPitches[Next() % (sizeof kPitches / sizeof kPitches[0])];
        if (pitch == 0) pitch = static_cast<std::int32_t>(w * BytesPerPixel());
        const unsigned char fill = static_cast<unsigned char>(Next());
        std::memset(g_dst[0], fill, kDstBytes);
        std::memset(g_dst[1], fill, kDstBytes);
        unsigned char* d0 = g_dst[0] + kDstBytes / 2 + (Next() % 4);
        unsigned char* d1 = g_dst[1] + (d0 - g_dst[0]);
        const unsigned char* src = g_source + (Next() % 8) * 4 + (Next() % 2) * 0x800 * (Next() % 8);
        const void* palette = g_palettes + (Next() % 64) * 32;
        if (which == 0) {
            ++c->glyph;
            if (BytesPerPixel() == 4) ++c->glyph_wide;
            g_their_glyph(d0, src, palette, pitch);
            Font_UnpackGlyph(d1, src, palette, pitch);
        } else {
            ++c->cell[which - 1];
            if (BytesPerPixel() == 4) ++c->cell_wide;
            g_their_cell[which - 1](d0, src, palette, static_cast<int>(w), static_cast<int>(h), pitch);
            kOurCell[which - 1](d1, src, palette, static_cast<int>(w), static_cast<int>(h), pitch);
        }
        if (std::memcmp(g_dst[0], g_dst[1], kDstBytes) != 0) {
            U at = 0;
            while (g_dst[0][at] == g_dst[1][at]) ++at;
            if (*bad < 4)
                bof3::Log("shadow      tex_cells MISMATCH: %s round %u (w %u h %u pitch %d, depth byte %u): byte 0x%X is "
                          "0x%02X, the original 0x%02X",
                          which == 0 ? "Font_UnpackGlyph" : which == 1 ? "Cell_Unpack4" : which == 2 ? "Cell_Unpack8"
                                                          : which == 3 ? "Cell_Unpack4Flip" : "Cell_Unpack8Flip",
                          r, (unsigned)w, (unsigned)h, (int)pitch, At(kPixelFormat)[3], (unsigned)at, g_dst[1][at],
                          g_dst[0][at]);
            ++*bad;
            Refuse();
        }
    }
}

// --- the fit, directly ---------------------------------------------------------------------------

unsigned FitRounds(unsigned rounds, unsigned* bad) {
    const U sides[] = {0, 1, 2, 3, 7, 8, 9, 24, 64, 65, 255, 256, 257, 320, 1023, 1024, 0x3FFFFFFF, 0x40000000,
                       0x80000000u, 0xFFFFFFFFu, 0xFFFF};
    unsigned squared = 0;
    for (unsigned r = 0; r < rounds; ++r) {
        RandomCaps(Next());
        if (Next() % 4 == 0) PutLong(kMinTexWidth, Next() % 3 ? Pick(sides) & 0x7FFFFFFF : 0x40000000);
        if (Next() % 4 == 0) PutLong(kMaxTexWidth, Pick(sides));
        if (Next() % 4 == 0) PutLong(kMaxTexHeight, Pick(sides));
        // A side above 0x40000000 as a positive int never ends the doubling
        // (in the original too): sides stay at or below it, or negative.
        U w = Next() % 2 ? Pick(sides) : Next() % 0x500, h = Next() % 2 ? Pick(sides) : Next() % 0x500;
        if (static_cast<std::int32_t>(w) > 0x40000000) w = 0x40000000;
        if (static_cast<std::int32_t>(h) > 0x40000000) h = 0x40000000;
        const bool alias = Next() % 8 == 0;
        U a[2] = {Next(), Next()}, b[2] = {a[0], a[1]};
        g_clone.fit(w, h, &a[0], alias ? &a[0] : &a[1]);
        D3d_FitTextureSize(w, h, &b[0], alias ? &b[0] : &b[1]);
        if ((At(kTexCaps)[0] & 0x20) != 0) ++squared;
        if (a[0] != b[0] || a[1] != b[1]) {
            if (*bad < 4)
                bof3::Log("shadow      tex_cells MISMATCH: D3d_FitTextureSize round %u (%u x %u, caps 0x%02X): %u x %u, "
                          "the original %u x %u",
                          r, (unsigned)w, (unsigned)h, At(kTexCaps)[0], (unsigned)b[0], (unsigned)b[1], (unsigned)a[0],
                          (unsigned)a[1]);
            ++*bad;
            Refuse();
        }
    }
    return squared;
}

// --- the clear and the free ----------------------------------------------------------------------

const U kSurfacePitches[] = {0, 0, 64, 128, 130, 640, 1280, 1296, 2048, 700, 1283};

void ClearRounds(unsigned rounds, unsigned* bad) {
    for (unsigned r = 0; r < rounds; ++r) {
        g_round = r;
        g_kind = kOtherRound;
        ddraw_fuzz::Reset(Next());
        RandomFormats();
        const U widths[] = {1, 24, 32, 256, 320};
        void* surface = ddraw_fuzz::MakeSurface(Pick(widths), 1 + Next() % 256, BytesPerPixel(), Pick(kSurfacePitches));
        if (Next() % 4 == 0) ddraw_fuzz::FailAt(ddraw_fuzz::kSurfaceCall + ddraw_fuzz::kBlt, 0);
        U theirs, ours;
        TwoPasses([&] { return static_cast<U>(g_clone.clear(surface)); },
                  [&] { return static_cast<U>(Dd_ClearSurface(surface)); }, &theirs, &ours);
        Compare("Dd_ClearSurface", r, ours, theirs, bad);
    }
}

const U kSlots[] = {0, 1, 2, 63, 126, 127, 128, 129};

void FreeRounds(unsigned rounds, unsigned* bad) {
    for (unsigned r = 0; r < rounds; ++r) {
        g_round = r;
        g_kind = kOtherRound;
        ddraw_fuzz::Reset(Next());
        void* a = ddraw_fuzz::MakeSurface(64, 64, 2, 0);
        void* b = ddraw_fuzz::MakeSurface(64, 64, 2, 0);
        RandomBytes(kCellCache, 0x1400);
        const U slot = Next() % 2 ? Pick(kSlots) : Next() % 128;
        const U entry = kCellCache + slot * kCellEntryBytes;
        PutLong(entry + 0x20, Next() % 3 ? Addr(Next() % 2 ? a : b) : 0);
        PutLong(entry + 0x24, Next() % 3 ? Addr(ddraw_fuzz::TextureOf(Next() % 2 ? a : b)) : 0);
        U theirs, ours;
        TwoPasses([&] { g_clone.free(static_cast<int>(slot)); return 0u; },
                  [&] { D3d_FreeCellTexture(static_cast<int>(slot)); return 0u; }, &theirs, &ours);
        Compare("D3d_FreeCellTexture", r, ours, theirs, bad);
    }
}

// --- the glyph ------------------------------------------------------------------------------------

struct GlyphCoverage {
    unsigned software, direct3d, made, reused, keyed, create_failed, lock_failed, overrun;
};

void GlyphRounds(unsigned rounds, unsigned* bad, GlyphCoverage* c) {
    const U glyphs[] = {0, 1, kGlyphs - 2, kGlyphs - 1};
    const U cluts[] = {0, 1, 0x3F, 0x40, 0x7FC0, 0xFFFF, 0x10000, 0x1FFC0, 0xFFFFFFC0u, 0xFFFFF000u, 0xFFFF0000u};
    const U slots[] = {0, 1, 126, 127, 128};
    g_src_base = Addr(g_glyphs);
    for (unsigned r = 0; r < rounds; ++r) {
        g_round = r;
        g_kind = kGlyphRound;
        ddraw_fuzz::Reset(Next());
        RandomFormats();
        const U bpp = BytesPerPixel();
        g_stage[0] = ddraw_fuzz::MakeSurface(320, 256, bpp, Pick(kSurfacePitches));
        g_stage[1] = ddraw_fuzz::MakeSurface(320, 256, bpp, Pick(kSurfacePitches));
        g_entry_surfaces[0] = ddraw_fuzz::MakeSurface(32, 32, bpp, Pick(kSurfacePitches));
        g_entry_surfaces[1] = ddraw_fuzz::MakeSurface(32, 32, bpp, Pick(kSurfacePitches));
        U pitches[2] = {Pick(kSurfacePitches), Pick(kSurfacePitches)};
        ddraw_fuzz::SetPitches(pitches, 2);
        PutLong(kStage, Addr(g_stage[0]));
        PutLong(kGlyphData, Addr(g_glyphs + (Next() % 2) * kGlyphs * kGlyphBytes));
        At(kRenderFlags)[0] = static_cast<unsigned char>(Next());
        RandomPalettes();
        RandomBytes(kFontCache, 0xA30);
        RandomBytes(kClutRows, 0x1000);
        const U slot = Next() % 3 == 0 ? Pick(slots) : Next() % 128;
        const U glyph = Next() % 3 == 0 ? Pick(glyphs) : Next() % kGlyphs;
        const U clut = Next() % 3 == 0 ? Pick(cluts) : Next() & 0xFFFF;
        g_entry = kFontCache + slot * kFontEntryBytes;
        g_generation = kClutRows + static_cast<U>(static_cast<std::int32_t>(clut) >> 6) * 8;
        if (g_generation < kClutRows || g_generation >= kClutRows + 0x1000) g_generation = kClutRows + 8 * (Next() % 512);
        const bool fresh = Next() % 2 == 0;
        PutLong(g_entry + 8, fresh ? 0 : Addr(g_entry_surfaces[Next() % 2]));
        if (Next() % 3 == 0) ddraw_fuzz::FailAt(ddraw_fuzz::kSurfaceCall + ddraw_fuzz::kLock, 0);
        if (Next() % 6 == 0) ddraw_fuzz::FailAt(ddraw_fuzz::kDirectDrawCall + ddraw_fuzz::kCreateSurface, 0);
        if (Next() % 8 == 0) ddraw_fuzz::FailAt(ddraw_fuzz::kSurfaceCall + ddraw_fuzz::kQueryInterface, 0);
        const bool software = At(kRenderFlags)[0] & 1;
        ++(software ? c->software : c->direct3d);
        ++(fresh ? c->made : c->reused);
        if (!software && fresh && (At(kRenderFlags)[0] & 0x20)) ++c->keyed;
        if (slot == 128) ++c->overrun;
        U theirs, ours;
        TwoPasses([&] { g_clone.glyph(static_cast<int>(slot), glyph, clut); return 0u; },
                  [&] { Font_BuildGlyphTexture(static_cast<int>(slot), glyph, clut); return 0u; }, &theirs, &ours);
        if (EndedAt(g_theirs, ddraw_fuzz::kDirectDrawCall + ddraw_fuzz::kCreateSurface)) ++c->create_failed;
        if (EndedAt(g_theirs, ddraw_fuzz::kSurfaceCall + ddraw_fuzz::kLock)) ++c->lock_failed;
        Compare("Font_BuildGlyphTexture", r, ours, theirs, bad);
    }
}

// --- the cells ---------------------------------------------------------------------------------------

struct CellCoverage {
    unsigned software, direct3d, backdrop, empty, one, many, most, flipped, bits8, scaled, create_failed, lock_failed,
        negative_first, spill;
};

const U kCellCluts[] = {0, 1, 0x3F, 0x40, 0x7FC0, 0xFFFF, 0x10000, 0x1FFC0};
constexpr U kRecords = 0x800;
constexpr U kMostCells = 100;

// A round's surfaces, flags, formats, palettes, caps and records; the round's
// first, count, CLUT and slot. Returns false for a round nothing could run.
struct CellRound {
    U slot, first, count, clut, backdrop;
};
CellRound CellWorld(bool refresh, CellCoverage* c) {
    ddraw_fuzz::Reset(Next());
    RandomFormats();
    const U bpp = BytesPerPixel();
    const U stage_pitches[] = {0, 0, 0, 640, 1280, 1296, 2048, 700, 1283};
    g_stage[0] = ddraw_fuzz::MakeSurface(320, 256, bpp, Pick(stage_pitches));
    g_stage[1] = ddraw_fuzz::MakeSurface(320, 256, bpp, Pick(stage_pitches));
    g_backdrop = ddraw_fuzz::MakeSurface(320, 240, Next() % 8 ? bpp : 6 - bpp, 0);
    g_entry_surfaces[0] = ddraw_fuzz::MakeSurface(256, 256, bpp, Pick(kSurfacePitches));
    g_entry_surfaces[1] = ddraw_fuzz::MakeSurface(128, 64, bpp, Pick(kSurfacePitches));
    U pitches[2] = {Pick(kSurfacePitches), Pick(kSurfacePitches)};
    ddraw_fuzz::SetPitches(pitches, 2);
    PutLong(kStage, Addr(g_stage[0]));
    At(kRenderFlags)[0] = static_cast<unsigned char>(Next());
    RandomPalettes();
    RandomBytes(kCellCache, 0x1528 - 0x2C);   // up to 0x7CC334, the DirectDraw globals
    RandomBytes(kClutRows, 0x1000);
    RandomCaps(Next());
    RandomBytes(0x6BE9F8, 0x20);
    PutLong(kBackdrop, Next() % 4 ? Addr(g_backdrop) : 0);

    CellRound round;
    round.slot = Next() % 3 == 0 ? Pick(kSlots) : Next() % 128;
    round.clut = Next() % 3 == 0 ? Pick(kCellCluts) : Next() & 0xFFFF;
    round.backdrop = Next() % 2 ? 0x800 : Next() % 4 ? 0 : Next();
    switch (Next() % 8) {
    case 0: round.count = 0; break;
    case 1: round.count = 1; break;
    case 2: round.count = kMostCells; break;
    default: round.count = 1 + Next() % 24; break;
    }
    switch (Next() % 10) {
    case 0: round.first = 0; break;
    case 1: round.first = kRecords - round.count; break;
    case 2:
        // Before the table: records -2 and -1 are the dwords 0x6BEA08..0x6BEA17
        // (the loop's test is signed).
        round.first = Next() % 2 ? 0xFFFFFFFFu : 0xFFFFFFFEu;
        round.count = 1 + Next() % 3;
        ++c->negative_first;
        break;
    case 3:
        // A first whose sum with the count wraps negative: the signed test
        // skips the loop; an unsigned one would draw record -1.
        round.first = 0x7FFFFFFFu;
        round.count = 1;
        ++c->negative_first;
        break;
    default: round.first = Next() % (kRecords - round.count + 1); break;
    }
    const bool spill = Next() % 16 == 0;
    if (spill) ++c->spill;
    for (U i = 0; i < kRecords; ++i) RandomRecord(kCellTable + i * 8, Next(), spill);
    RandomRecord(0x6BEA08, Next(), false);
    RandomRecord(0x6BEA10, Next(), false);
    if (round.first < kRecords)
        for (U i = round.first; i < round.first + round.count; ++i) {
            const U tpage = At(kCellTable + i * 8 + 4)[0] | At(kCellTable + i * 8 + 5)[0] << 8;
            if (tpage & 0x200) ++c->flipped;
            if (tpage & 0x180) ++c->bits8;
        }

    g_entry = kCellCache + round.slot * kCellEntryBytes;
    g_first = round.first;
    g_count = round.count;
    g_generation = kClutRows + (round.clut >> 6) * 8;
    if (g_generation >= kClutRows + 0x1000) g_generation = kClutRows + 8 * (Next() % 512);
    PutLong(g_entry + 0x20, refresh || Next() % 3 ? Addr(g_entry_surfaces[Next() % 2]) : 0);
    PutLong(g_entry + 0x24, Next() % 3 ? Addr(ddraw_fuzz::TextureOf(g_entry_surfaces[Next() % 2])) : 0);
    if (refresh) {
        PutWord(g_entry + 0, Next() % 330);
        PutWord(g_entry + 2, Next() % 270);
        PutWord(g_entry + 8, Range(-160, 0));
        PutWord(g_entry + 0xA, Range(0, 160));
        PutWord(g_entry + 0xC, Range(-128, 0));
        PutWord(g_entry + 0xE, Range(0, 128));
    }
    if (Next() % 5 == 0) ddraw_fuzz::FailAt(ddraw_fuzz::kSurfaceCall + ddraw_fuzz::kLock, 0);
    if (Next() % 6 == 0) ddraw_fuzz::FailAt(ddraw_fuzz::kDirectDrawCall + ddraw_fuzz::kCreateSurface, 0);
    if (Next() % 8 == 0) ddraw_fuzz::FailAt(ddraw_fuzz::kSurfaceCall + ddraw_fuzz::kQueryInterface, 0);
    if (Next() % 8 == 0) ddraw_fuzz::FailAt(ddraw_fuzz::kSurfaceCall + ddraw_fuzz::kBlt, Next() % 2);

    if (round.count == 0) ++c->empty;
    else if (round.count == 1) ++c->one;
    else if (round.count == kMostCells) ++c->most;
    else ++c->many;
    const bool software = At(kRenderFlags)[0] & 1;
    ++(software ? c->software : c->direct3d);
    if (!software && round.backdrop && Long(kBackdrop)) ++c->backdrop;
    return round;
}

void BuildRounds(unsigned rounds, unsigned* bad, CellCoverage* c) {
    g_src_base = kVram;
    for (unsigned r = 0; r < rounds; ++r) {
        g_round = r;
        g_kind = kBuildRound;
        const CellRound round = CellWorld(false, c);
        g_control_word = Pick(kControlWords);
        const ControlWord cw(static_cast<unsigned short>(g_control_word));
        U theirs, ours;
        TwoPasses(
            [&] {
                g_clone.build(static_cast<int>(round.slot), round.first, round.count, round.clut, round.backdrop);
                return 0u;
            },
            [&] {
                D3d_BuildCellTexture(static_cast<int>(round.slot), round.first, round.count, round.clut, round.backdrop);
                return 0u;
            },
            &theirs, &ours);
        if (EndedAt(g_theirs, ddraw_fuzz::kDirectDrawCall + ddraw_fuzz::kCreateSurface)) ++c->create_failed;
        if (EndedAt(g_theirs, ddraw_fuzz::kSurfaceCall + ddraw_fuzz::kLock)) ++c->lock_failed;
        // Scaled: the texture came out narrower than the extent - the Blt after
        // EndScene has a destination (a[1..4]) narrower than its source (a[6..9]).
        bool scene = false;
        for (unsigned i = 0; i < g_theirs.n && i < ddraw_fuzz::kMaxCalls; ++i) {
            const ddraw_fuzz::Call& call = g_theirs.calls[i];
            if (call.what == kEndSceneCall) scene = true;
            if (scene && call.what == ddraw_fuzz::kSurfaceCall + ddraw_fuzz::kBlt) {
                if (call.a[3] < call.a[8] - call.a[6]) ++c->scaled;
                break;
            }
        }
        Compare("D3d_BuildCellTexture", r, ours, theirs, bad);
    }
}

void RefreshRounds(unsigned rounds, unsigned* bad, CellCoverage* c) {
    g_src_base = kVram;
    for (unsigned r = 0; r < rounds; ++r) {
        g_round = r;
        g_kind = kRefreshRound;
        const CellRound round = CellWorld(true, c);
        U theirs, ours;
        TwoPasses(
            [&] {
                g_clone.refresh(static_cast<int>(round.slot), round.first, round.count, round.clut);
                return 0u;
            },
            [&] {
                D3d_RefreshCellTexture(static_cast<int>(round.slot), round.first, round.count, round.clut);
                return 0u;
            },
            &theirs, &ours);
        if (EndedAt(g_theirs, ddraw_fuzz::kSurfaceCall + ddraw_fuzz::kLock)) ++c->lock_failed;
        Compare("D3d_RefreshCellTexture", r, ours, theirs, bad);
    }
}

}  // namespace

void SelfTest() {
    MakeClones();
    BuildDevice();
    std::memcpy(g_vram_saved, At(kVram), kVramBytes);
    Capture(g_saved);
    g_rand = 0x5A32B0u;
    for (U i = 0; i < kVramBytes; i += 4) PutLong(kVram + i, Next());
    g_source = static_cast<unsigned char*>(VirtualAlloc(nullptr, kSourceBytes + 0x1000, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE));
    g_dst[0] = static_cast<unsigned char*>(VirtualAlloc(nullptr, kDstBytes, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE));
    g_dst[1] = static_cast<unsigned char*>(VirtualAlloc(nullptr, kDstBytes, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE));
    g_glyphs = static_cast<unsigned char*>(VirtualAlloc(nullptr, 2 * kGlyphs * kGlyphBytes, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE));
    if (!g_source || !g_dst[0] || !g_dst[1] || !g_glyphs)
        bof3::Fatal("tex_cells: VirtualAlloc failed, error %lu", GetLastError());
    for (U i = 0; i < kSourceBytes; i += 4) PutLong(Addr(g_source + i), Next());
    for (U i = 0; i < 2 * kGlyphs * kGlyphBytes; i += 4) PutLong(Addr(g_glyphs + i), Next());

    unsigned bad = 0;
    UnpackCoverage uc = {};
    constexpr unsigned kUnpackRounds = 6000;
    UnpackRounds(kUnpackRounds, &bad, &uc);
    constexpr unsigned kFitRounds = 20000;
    const unsigned squared = FitRounds(kFitRounds, &bad);

    constexpr unsigned kClearRounds = 1000, kFreeRounds = 2000, kGlyphRounds = 4000, kBuildRounds = 5000,
                       kRefreshRounds = 4000;
    GlyphCoverage gc = {};
    CellCoverage bc = {}, rc = {};
    {
        const ddraw_fuzz::GlobalSwap dd(ddraw_fuzz::kDirectDraw, ddraw_fuzz::FakeDirectDraw());
        const ddraw_fuzz::GlobalSwap device(kDevice, &g_device);
        const ddraw_fuzz::GlobalSwap glyphs(kGlyphData, g_glyphs);
        ddraw_fuzz::SetDisturb(nullptr);
        ClearRounds(kClearRounds, &bad);
        FreeRounds(kFreeRounds, &bad);
        ddraw_fuzz::SetDisturb(&Disturb);
        GlyphRounds(kGlyphRounds, &bad, &gc);
        BuildRounds(kBuildRounds, &bad, &bc);
        RefreshRounds(kRefreshRounds, &bad, &rc);
        ddraw_fuzz::SetDisturb(nullptr);
        g_kind = kOtherRound;
    }
    Restore(g_saved);
    std::memcpy(At(kVram), g_vram_saved, kVramBytes);
    VirtualFree(g_source, 0, MEM_RELEASE);
    VirtualFree(g_dst[0], 0, MEM_RELEASE);
    VirtualFree(g_dst[1], 0, MEM_RELEASE);
    VirtualFree(g_glyphs, 0, MEM_RELEASE);

    bof3::Log("shadow      tex_cells self-test: unpackers %u rounds (Font_UnpackGlyph %u, Cell_Unpack4 %u, Cell_Unpack8 %u, "
              "Cell_Unpack4Flip %u, Cell_Unpack8Flip %u; at 4 bytes a texel %u glyph, %u cell), D3d_FitTextureSize %u (%u "
              "square-only), Dd_ClearSurface %u, D3d_FreeCellTexture %u",
              kUnpackRounds, uc.glyph, uc.cell[0], uc.cell[1], uc.cell[2], uc.cell[3], uc.glyph_wide, uc.cell_wide,
              kFitRounds, squared, kClearRounds, kFreeRounds);
    bof3::Log("shadow      tex_cells self-test: Font_BuildGlyphTexture %u rounds (software %u, Direct3D %u, surface made %u, "
              "reused %u, colour-keyed %u, slot 128 %u, CreateSurface failed %u, Lock failed %u)",
              kGlyphRounds, gc.software, gc.direct3d, gc.made, gc.reused, gc.keyed, gc.overrun, gc.create_failed,
              gc.lock_failed);
    bof3::Log("shadow      tex_cells self-test: D3d_BuildCellTexture %u rounds (software %u, Direct3D %u, backdrop %u, "
              "0 cells %u, 1 %u, 2..24 %u, %u cells %u, before the table %u, spilling %u; flipped cells %u, 8-bit %u; "
              "scaled %u (%u sizes seeded for the precision), CreateSurface failed %u, Lock failed %u)",
              kBuildRounds, bc.software, bc.direct3d, bc.backdrop, bc.empty, bc.one, bc.many, kMostCells, bc.most,
              bc.negative_first, bc.spill, bc.flipped, bc.bits8, bc.scaled, g_precision_seeded / 2, bc.create_failed,
              bc.lock_failed);
    bof3::Log("shadow      tex_cells self-test: D3d_RefreshCellTexture %u rounds (software %u, Direct3D %u, 0 cells %u, 1 %u, "
              "2..24 %u, %u cells %u, before the table %u; flipped cells %u, 8-bit %u; Lock failed %u)",
              kRefreshRounds, rc.software, rc.direct3d, rc.empty, rc.one, rc.many, kMostCells, rc.most, rc.negative_first,
              rc.flipped, rc.bits8, rc.lock_failed);
    if (bad) bof3::Fatal("the glyph and cell textures differ from the original in %u self-test rounds", bad);
}

}  // namespace tex_cells
