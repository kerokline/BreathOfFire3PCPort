// BOF3X_SHADOW=d3d_draw: a differential fuzz of the Direct3D draw handlers, their
// state helpers and the two texture lookups against byte-copies of Capcom's,
// once at start-up, on the vertex-block harness (d3d_fuzz.h). docs/d3d-draw.md
// section 5.
//
// Every copy has each of its calls re-aimed at a recording stand-in, the same
// stand-ins ours is put on through d3d_draw::g; the device is the harness's
// fake. Per round: random state (the primitive, the scales, the vertex block,
// the texture-coordinate table, Gfx_DrawTpage, the caches), boundaries seeded;
// Capcom's copy, then from the same state ours; the log of calls, the vertices
// each DrawPrimitive was handed, every byte of state either could touch and the
// return value compared. The x87 handlers run under a control word picked per
// round from 0x027F (the game's, measured), 0x007F and 0x037F.
//
// The stand-ins write what the real callees write where the caller reads it
// again (the colour pair, a cache entry, the render flag) - and, now and then,
// a byte of the primitive, the vertex block, the scales, Gfx_DrawTpage or a
// texture coordinate: so every read and every store the handler makes is
// ordered against every call it makes, and an order changed shows.
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <initializer_list>

#include "bof3/symbols.gen.h"
#include "game/d3d_draw_callees.h"
#include "game/d3d_fuzz.h"
#include "hook/detour.h"
#include "hook/log.h"

namespace d3d_draw {
namespace {

using d3d_fuzz::Next;
using d3d_fuzz::Pick;
using d3d_fuzz::Record;

unsigned char* At(U address) { return reinterpret_cast<unsigned char*>(static_cast<std::uintptr_t>(address)); }
void PutWord(unsigned char* p, U v) {
    const std::uint16_t w = static_cast<std::uint16_t>(v);
    std::memcpy(p, &w, sizeof w);
}
void PutLong(unsigned char* p, U v) { std::memcpy(p, &v, sizeof v); }
U GetLong(const unsigned char* p) {
    U v;
    std::memcpy(&v, p, sizeof v);
    return v;
}
U GetWord(const unsigned char* p) {
    std::uint16_t v;
    std::memcpy(&v, p, sizeof v);
    return v;
}

unsigned short GetControlWord() {
    unsigned short cw;
    __asm__ volatile("fnstcw %0" : "=m"(cw));
    return cw;
}
void SetControlWord(unsigned short cw) { __asm__ volatile("fldcw %0" : : "m"(cw)); }
const unsigned short kControlWords[] = {0x027F, 0x007F, 0x037F};

// --- the state -------------------------------------------------------------

// The D27 victim can be an index up to the largest `count` the fuzz passes, so
// the cell cache region runs 5 entries past the table.
constexpr U kCellCountMax = 0x84;
constexpr U kCellRegionBytes = (kCellCountMax + 1) * kCellEntry;
constexpr U kCellsBytes = 0x2000;
constexpr U kPageCacheBytes = 32 * 32 * 0x18;

struct Region {
    U address, bytes;
};
const Region kRegions[] = {
    {kVertices, 0x80},        {kScaleY, 8},        {kTexCoords, 0x400}, {kDrawTpage, 4},
    {kShadeCache, 4},         {kRenderFlags, 1},   {kPageCache, kPageCacheBytes},
    {kClutRows, 512 * 8},     {kCells, kCellsBytes}, {kCellCache, kCellRegionBytes},
};
constexpr U kStateBytes = 0x80 + 8 + 0x400 + 4 + 4 + 1 + kPageCacheBytes + 512 * 8 + kCellsBytes + kCellRegionBytes;

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
// Which region differs, for the log.
const char* FirstDifference(const State& a, const State& b, U* where) {
    U at = 0;
    for (const Region& r : kRegions) {
        for (U i = 0; i < r.bytes; ++i)
            if (a.bytes[at + i] != b.bytes[at + i]) {
                *where = r.address + i;
                return "memory";
            }
        at += r.bytes;
    }
    return nullptr;
}

// --- the stand-ins -----------------------------------------------------------

U g_round;
unsigned char* g_prim;
U g_prim_bytes;
const unsigned char kTexels[] = {0x00, 0x01, 0x7F, 0x80, 0xFE, 0xFF};

U Mix(U salt) {
    U h = (g_round * 0x9E3779B1u) ^ (salt * 0x85EBCA6Bu) ^ (d3d_fuzz::g_log->n * 0xC2B2AE35u);
    h ^= h >> 16;
    h *= 0x7FEB352Du;
    h ^= h >> 15;
    h *= 0x846CA68Bu;
    h ^= h >> 16;
    return h;
}

// A quarter of the time, one byte of what the handlers read or write.
void Disturb(U salt) {
    const U h = Mix(salt ^ 0x5BD1E995u);
    if (h % 4) return;
    const auto value = static_cast<unsigned char>(h >> 24);
    switch ((h >> 2) % 7) {
    case 0:
    case 1:
        if (g_prim) g_prim[(h >> 8) % g_prim_bytes] = value;
        break;
    case 2: At(kVertices)[(h >> 8) % 0x80] = value; break;
    case 3: At(kScaleY)[(h >> 8) % 8] = value; break;
    case 4: At(kDrawTpage)[(h >> 8) % 2] = value; break;
    case 5: At(kTexCoords + kTexels[(h >> 8) % 6] * 4u)[(h >> 16) % 4] = value; break;
    default: At(kRenderFlags)[0] ^= 1; break;
    }
}

void __cdecl StubPrimColor(unsigned r, unsigned gg, unsigned b, unsigned code, unsigned mode,
                           unsigned long* diffuse, unsigned long* specular) {
    Record(1, r, gg, b, code, mode, diffuse != nullptr, specular != nullptr);
    Disturb(1);
    *diffuse = Mix(2);
    if (specular) *specular = Mix(3);
}
long __cdecl StubBind(unsigned tpage, unsigned clut) {
    Record(2, tpage, clut);
    Disturb(4);
    return static_cast<long>(Mix(5));
}
void __cdecl StubRet(unsigned a) {
    Record(3, a);
    Disturb(6);
}
void __cdecl StubBlend(unsigned code, unsigned mode) {
    Record(4, code, mode);
    Disturb(7);
}
void __cdecl StubShade(unsigned mode) {
    Record(5, mode);
    Disturb(8);
}
int __cdecl StubCellTexture(unsigned first, unsigned count, unsigned clut, unsigned flags) {
    Record(6, first, count, clut, flags);
    Disturb(9);
    return static_cast<int>(Mix(10) % (kCellEntries + 1));   // 0x80: one past the table, as D27 can
}

// The page texture cache's callees. Find returns a slot and sometimes changes
// the entry it names (state, texture) - the caller reads both after it.
int __cdecl StubFind(int page, int clut, int mode) {
    Record(7, static_cast<U>(page), static_cast<U>(clut), static_cast<U>(mode));
    const U h = Mix(11);
    const int slot = h % 3 == 0 ? 0x20 : static_cast<int>((h >> 4) % 32);
    if (slot < 0x20 && page >= 0 && page < 32 && (h >> 12) % 4 == 0) {
        unsigned char* e = At(kPageCache + static_cast<U>(page * 32 + slot) * 0x18);
        e[0] = static_cast<unsigned char>((h >> 16) % 4);
        PutLong(e + 0x14, Mix(12));
    }
    return slot;
}
unsigned long __cdecl StubBuild(int page, int clut, int mode) {
    Record(8, static_cast<U>(page), static_cast<U>(clut), static_cast<U>(mode));
    return Mix(13);
}
// The refresh writes a new texture: the caller hands SetTexture the OLD one.
void __cdecl StubRefresh(int page, int slot) {
    Record(9, static_cast<U>(page), static_cast<U>(slot));
    if (page >= 0 && page < 32 && slot >= 0 && slot < 32) {
        unsigned char* e = At(kPageCache + static_cast<U>(page * 32 + slot) * 0x18);
        PutLong(e + 0x14, Mix(14));
        e[0] = 1;
    }
}

// The cell cache's callees: the build fills the entry as the real one's success
// path does; both write a new texture; both now and then flip the render flag,
// which the caller reads after them.
void FillCellEntry(int slot, unsigned first, unsigned count, unsigned clut, bool whole) {
    if (slot < 0 || static_cast<U>(slot) > kCellCountMax) return;
    unsigned char* e = At(kCellCache + static_cast<U>(slot) * kCellEntry);
    if (whole) {
        for (U k = 0; k < 0x10; k += 4) PutLong(e + k, Mix(20 + k));
        PutWord(e + 0x10, clut);
        PutWord(e + 0x12, count);
        U sum = 0;
        for (U k = 0; k < count * 8; k += 4) sum += GetLong(At(kCells + first * 8 + k));
        PutLong(e + 0x18, sum);
        PutLong(e + 0x1C, GetLong(At(kClutRows + ((clut & 0xFFFF) >> 6) * 8)));
        PutLong(e + 0x20, Mix(15));
        if (Mix(16) % 2) PutWord(e + 0x16, Mix(17) % 4);
    }
    PutLong(e + 0x24, Mix(18));
    if (Mix(19) % 4 == 0) At(kRenderFlags)[0] ^= 1;
}
void __cdecl StubCellBuild(int slot, unsigned first, unsigned count, unsigned clut, unsigned flip) {
    Record(10, static_cast<U>(slot), first, count, clut, flip);
    FillCellEntry(slot, first, count, clut, true);
}
void __cdecl StubCellRefresh(int slot, unsigned first, unsigned count, unsigned clut) {
    Record(11, static_cast<U>(slot), first, count, clut);
    FillCellEntry(slot, first, count, clut, false);
}

const Callees kStandIns = {StubPrimColor, StubBind,       StubRet,   StubBlend,     StubShade,       StubCellTexture,
                           StubFind,      StubBuild,      StubRefresh, StubCellBuild, StubCellRefresh};

// --- the copies --------------------------------------------------------------


template <typename F> const void* P(F* f) { return reinterpret_cast<const void*>(f); }

// A copy whose jump table is moved into it: the table's entries and the
// `jmp [reg*4 + table]` operand re-aimed (HANDOFF traps: a copy with its table
// left in place runs its cases in the original).
void* CloneWithTable(const char* name, U original, U size, U disp_at, U table_at, U entries) {
    auto* code = static_cast<unsigned char*>(bof3::CloneOriginal(name, original, size));
    if (!code) bof3::Fatal("d3d_draw: CloneOriginal(%s) returned null", name);
    const U moved = static_cast<U>(reinterpret_cast<std::uintptr_t>(code)) - original;
    for (U i = 0; i < entries; ++i) {
        U target = GetLong(code + table_at + 4 * i);
        if (target < original || target >= original + table_at)
            bof3::Fatal("%s: jump table entry %u is 0x%X", name, (unsigned)i, (unsigned)target);
        PutLong(code + table_at + 4 * i, target + moved);
    }
    U disp = GetLong(code + disp_at);
    if (disp != original + table_at) bof3::Fatal("%s: no jump table operand at +0x%X", name, (unsigned)disp_at);
    PutLong(code + disp_at, disp + moved);
    return code;
}

template <typename Fn> Fn Clone(const char* name, U original, U size, const bof3::CloneCall* calls, int n) {
    void* code = bof3::CloneOriginal(name, original, size, calls, n);
    if (!code) bof3::Fatal("d3d_draw: CloneOriginal(%s) returned null", name);
    return reinterpret_cast<Fn>(code);
}

using ColorFn = void(__cdecl*)(unsigned, unsigned, unsigned, unsigned, unsigned, unsigned long*, unsigned long*);
using BlendFn = void(__cdecl*)(unsigned, unsigned);
using ShadeFn = void(__cdecl*)(unsigned);
using BindFn = long(__cdecl*)(unsigned, unsigned);
using DrawFn = long(__cdecl*)(const unsigned char*);
using CellFn = int(__cdecl*)(unsigned, unsigned, unsigned, unsigned);

// The handlers: every E8 of each body, from the disassembly (docs/d3d-draw.md
// section 2); every other transfer is internal or through the device.
struct Handler {
    const char* name;
    U original, size;
    DrawFn ours;
    U prim_bytes;
    bool gouraud_colours;   // only for the log
    bof3::CloneCall calls[10];
    int n_calls;
    // where the floats, the u / v bytes and the words live, for seeding
    unsigned char floats[16];
    unsigned char n_floats;
    unsigned char texels[8];
    unsigned char n_texels;
    unsigned char words[4];
    unsigned char n_words;
};

#define CALL(off, stub, addr) {off, P(stub), addr}
const U kPrim = 0x59FBA0, kBindAt = 0x59FFE0, kBlend = 0x59FCA0, kShade = 0x59FD80, kCellAt = 0x5A3160;

Handler g_handlers[] = {
    {"D3d_DrawPolyFT4", 0x5A0C40, 0x23D, D3d_DrawPolyFT4, 0x48, false,
     {CALL(0x2F, StubPrimColor, kPrim), CALL(0x1DE, StubBind, kBindAt), CALL(0x1EE, StubRet, kRetOnly),
      CALL(0x1FE, StubRet, kRetOnly), CALL(0x210, StubBlend, kBlend), CALL(0x217, StubShade, kShade)},
     6,
     {0x08, 0x0C, 0x10, 0x18, 0x1C, 0x20, 0x28, 0x2C, 0x30, 0x38, 0x3C, 0x40}, 12,
     {0x14, 0x15, 0x24, 0x25, 0x34, 0x35, 0x44, 0x45}, 8,
     {0x16, 0x26}, 2},
    {"D3d_DrawPolyGT4", 0x5A14C0, 0x2D4, D3d_DrawPolyGT4, 0x54, true,
     {CALL(0x31, StubPrimColor, kPrim), CALL(0x5F, StubPrimColor, kPrim), CALL(0x8D, StubPrimColor, kPrim),
      CALL(0xBE, StubPrimColor, kPrim), CALL(0x285, StubBind, kBindAt), CALL(0x28C, StubRet, kRetOnly),
      CALL(0x293, StubRet, kRetOnly), CALL(0x2A5, StubBlend, kBlend), CALL(0x2AC, StubShade, kShade)},
     9,
     {0x08, 0x0C, 0x10, 0x1C, 0x20, 0x24, 0x30, 0x34, 0x38, 0x44, 0x48, 0x4C}, 12,
     {0x14, 0x15, 0x28, 0x29, 0x3C, 0x3D, 0x50, 0x51}, 8,
     {0x16, 0x2A}, 2},
    {"D3d_DrawLineF2", 0x5A17A0, 0x10F, D3d_DrawLineF2, 0x20, false,
     {CALL(0x31, StubPrimColor, kPrim), CALL(0xC6, StubRet, kRetOnly), CALL(0xCD, StubRet, kRetOnly),
      CALL(0xE3, StubBlend, kBlend), CALL(0xEA, StubShade, kShade)},
     5,
     {0x08, 0x0C, 0x10, 0x14, 0x18, 0x1C}, 6,
     {}, 0,
     {}, 0},
    {"D3d_DrawLineF4", 0x5A1D10, 0x185, D3d_DrawLineF4, 0x38, false,
     {CALL(0x31, StubPrimColor, kPrim), CALL(0x13C, StubRet, kRetOnly), CALL(0x143, StubRet, kRetOnly),
      CALL(0x159, StubBlend, kBlend), CALL(0x160, StubShade, kShade)},
     5,
     {0x08, 0x0C, 0x10, 0x14, 0x18, 0x1C, 0x20, 0x24, 0x28, 0x2C, 0x30, 0x34}, 12,
     {}, 0,
     {}, 0},
    {"D3d_DrawTile", 0x5A20D0, 0x14A, D3d_DrawTile, 0x1C, false,
     {CALL(0x31, StubPrimColor, kPrim), CALL(0x101, StubRet, kRetOnly), CALL(0x108, StubRet, kRetOnly),
      CALL(0x11E, StubBlend, kBlend), CALL(0x125, StubShade, kShade)},
     5,
     {0x08, 0x0C, 0x10, 0x14, 0x18}, 5,
     {}, 0,
     {}, 0},
    {"D3d_DrawCellSprite", 0x5A2EB0, 0x2AA, D3d_DrawCellSprite, 0x20, false,
     {CALL(0x31, StubPrimColor, kPrim), CALL(0x52, StubCellTexture, kCellAt), CALL(0x269, StubRet, kRetOnly),
      CALL(0x27B, StubBlend, kBlend), CALL(0x282, StubShade, kShade)},
     5,
     {0x08, 0x0C, 0x10, 0x14}, 4,
     {}, 0,
     {0x18, 0x1A, 0x1C, 0x1E}, 4},
};
#undef CALL

// --- seeds -------------------------------------------------------------------

const U kFloats[] = {
    0x00000000, 0x80000000, 0x3F800000, 0x3F000000, 0x43200000, 0xC2000000, 0x3C23D70A,  // 0 -0 1 .5 160 -32 .01
    0x7149F2CA, 0x7F7FFFFF, 0x00800000, 0x007FFFFF, 0x00000001, 0x7F800000, 0xFF800000,  // 1e30 max min-normal denormals inf
    0x7FC00000, 0x7FA00000, 0xFF800001, 0x3F7D70A4, 0x4B7FFFFF, 0x3EAAAAAB, 0xBF800000,  // qNaN sNaN sNaN .99 2^24-1 1/3 -1
};
const U kScales[] = {0x40000000, 0x3F800000, 0x3FC00000, 0x40400000, 0x3F000000, 0xC0000000, 0x40100000,
                     0x0DA24260, 0x7F000000, 0x3F800001};
const U kWords[] = {0x0000, 0x0001, 0x0020, 0x0040, 0x0060, 0x0080, 0x0100, 0x0180, 0x0400,
                    0x0800, 0x0C00, 0x7FFF, 0x8000, 0xFFFF, 0x0010, 0x001F};
const U kColours[] = {0x00, 0x01, 0x7F, 0x80, 0x81, 0xFF};

void RandomFloat(unsigned char* p) {
    const U v = Next() % 3 ? Pick(kFloats, sizeof kFloats / 4) : Next();
    PutLong(p, v);
}

void RandomCommon() {
    for (U a : {kScaleX, kScaleY}) {
        U v;
        switch (Next() % 5) {
        case 0: v = Next(); break;
        case 1: v = 0x3F800000u + (Next() % 0x2000000u); break;
        default: v = Pick(kScales, sizeof kScales / 4); break;
        }
        PutLong(At(a), v);
    }
    for (U i = 0; i < 0x80; ++i) At(kVertices)[i] = static_cast<unsigned char>(Next());
    for (U i = 0; i < 256; ++i) {
        U v;
        if (Next() % 8) {
            const float f = (static_cast<float>(i) + 0.512f) / 256.0f;
            std::memcpy(&v, &f, 4);
        } else {
            v = Next() % 2 ? Next() : Pick(kFloats, sizeof kFloats / 4);
        }
        PutLong(At(kTexCoords + i * 4), v);
    }
    PutLong(At(kDrawTpage), Next() % 2 ? Next() : Pick(kWords, 16) | (Next() & 0xFFFF0000u));
    PutLong(At(kShadeCache), Next() % 4);
    At(kRenderFlags)[0] = static_cast<unsigned char>(Next());
}

void RandomPrim(const Handler& h, unsigned char* p) {
    for (U i = 0; i < h.prim_bytes; ++i) p[i] = static_cast<unsigned char>(Next());
    for (U i = 4; i < 7; ++i)
        if (Next() % 2) p[i] = static_cast<unsigned char>(Pick(kColours, 6));
    if (h.gouraud_colours)
        for (U c = 1; c < 4; ++c)
            for (U i = 0; i < 3; ++i)
                if (Next() % 2) p[4 + c * 0x14 + i] = static_cast<unsigned char>(Pick(kColours, 6));
    for (U i = 0; i < h.n_floats; ++i)
        if (Next() % 4) RandomFloat(p + h.floats[i]);
    for (U i = 0; i < h.n_texels; ++i)
        if (Next() % 2) p[h.texels[i]] = kTexels[Next() % 6];
    for (U i = 0; i < h.n_words; ++i)
        if (Next() % 2) PutWord(p + h.words[i], Pick(kWords, 16));
}

// The cell cache for the cell sprite: the entry the stand-in will name gets
// seeded sizes (0 among them: a division by zero) and extents.
void RandomCellCache() {
    for (U i = 0; i < kCellRegionBytes; ++i) At(kCellCache)[i] = static_cast<unsigned char>(Next());
    const U sizes[] = {0, 1, 2, 8, 16, 24, 32, 64, 256, 0x8000, 0xFFFF};
    const U extents[] = {0, 1, 0xFFFF, 0x7FFF, 0x8000, 8, 0xFFF8, 16};
    for (U k = 0; k <= kCellEntries; ++k) {
        unsigned char* e = At(kCellCache + k * kCellEntry);
        for (U f = 0; f < 8; f += 2)
            if (Next() % 2) PutWord(e + f, Pick(sizes, 11));
        for (U f = 8; f < 0x10; f += 2)
            if (Next() % 2) PutWord(e + f, Pick(extents, 8));
    }
}

d3d_fuzz::Log g_theirs, g_ours;

// Compares one pass's results. Returns false and fills `why`.
bool Same(long ret_ours, long ret_theirs, char* why) {
    if (!d3d_fuzz::SameLog(g_ours, g_theirs, why)) return false;
    if (ret_ours != ret_theirs) {
        std::strcpy(why, "the return value");
        return false;
    }
    Capture(g_ours_state);
    U where = 0;
    if (FirstDifference(g_ours_state, g_theirs_state, &where)) {
        std::snprintf(why, 160, "memory at 0x%X", (unsigned)where);
        return false;
    }
    return true;
}

// --- the handlers ------------------------------------------------------------

unsigned FuzzHandler(const Handler& h, DrawFn theirs, unsigned rounds) {
    unsigned bad = 0;
    unsigned char prim[0x60], prim_start[0x60], prim_theirs[0x60];
    const unsigned short saved_cw = GetControlWord();
    for (unsigned r = 0; r < rounds; ++r) {
        g_round = r + (h.original << 4);
        RandomCommon();
        if (h.original == 0x5A2EB0) RandomCellCache();
        RandomPrim(h, prim_start);
        const unsigned short cw = kControlWords[Next() % 3];
        Capture(g_start);

        std::memcpy(prim, prim_start, sizeof prim);
        g_prim = prim;
        g_prim_bytes = h.prim_bytes;
        g_theirs.Clear();
        d3d_fuzz::g_log = &g_theirs;
        SetControlWord(cw);
        const long ret_theirs = theirs(prim);
        SetControlWord(saved_cw);
        Capture(g_theirs_state);
        std::memcpy(prim_theirs, prim, sizeof prim);

        Restore(g_start);
        std::memcpy(prim, prim_start, sizeof prim);
        g_ours.Clear();
        d3d_fuzz::g_log = &g_ours;
        SetControlWord(cw);
        const long ret_ours = h.ours(prim);
        SetControlWord(saved_cw);
        d3d_fuzz::g_log = nullptr;
        g_prim = nullptr;

        char why[200] = "";
        bool same = Same(ret_ours, ret_theirs, why);
        if (same && std::memcmp(prim, prim_theirs, sizeof prim) != 0) {
            same = false;
            std::strcpy(why, "the primitive");
        }
        if (!same) {
            if (bad < 4) bof3::Log("shadow      d3d_draw MISMATCH: %s round %u (cw %04X): %s", h.name, r, cw, why);
            ++bad;
        }
    }
    return bad;
}

// --- D3d_PrimColor, D3d_SetBlend, D3d_SetShadeMode ---------------------------

const U kColourArgs[] = {0x00, 0x01, 0x7F, 0x80, 0x81, 0xFF, 0x100, 0x17F, 0x7FFFFFFF, 0x80000000, 0x80000080,
                         0xFFFFFFFF, 0x40};

U ColourArg() { return Next() % 3 ? Pick(kColourArgs, 13) : (Next() % 2 ? Next() & 0xFF : Next()); }
U ModeArg() {
    switch (Next() % 3) {
    case 0: return Next();
    case 1: return Pick(kWords, 16);
    default: return (Next() % 4) << 5;
    }
}

unsigned FuzzColour(ColorFn theirs, unsigned rounds) {
    unsigned bad = 0;
    for (unsigned r = 0; r < rounds; ++r) {
        const U red = ColourArg(), green = ColourArg(), blue = ColourArg();
        const U code = Next() % 2 ? (Next() & 0xFFFFFF00u) | (Next() % 8) : Next();
        const U mode = ModeArg();
        const U shape = Next() % 4;   // 0 null specular, 1 aliased, else two
        const U fill_d = Next(), fill_s = Next();
        unsigned long td = fill_d, ts = fill_s, od = fill_d, os = fill_s;
        theirs(red, green, blue, code, mode, &td, shape == 0 ? nullptr : shape == 1 ? &td : &ts);
        D3d_PrimColor(red, green, blue, code, mode, &od, shape == 0 ? nullptr : shape == 1 ? &od : &os);
        if (td != od || ts != os) {
            if (bad < 4)
                bof3::Log("shadow      d3d_draw MISMATCH: D3d_PrimColor(%X, %X, %X, %X, %X) shape %u: %lX %lX, "
                          "the original %lX %lX",
                          (unsigned)red, (unsigned)green, (unsigned)blue, (unsigned)code, (unsigned)mode,
                          (unsigned)shape, od, os, td, ts);
            ++bad;
        }
    }
    return bad;
}

unsigned FuzzBlendShade(BlendFn blend, ShadeFn shade, unsigned rounds) {
    unsigned bad = 0;
    for (unsigned r = 0; r < rounds; ++r) {
        g_round = 0x200000u + r;
        const bool which = r % 2;
        const U code = Next() % 2 ? Next() : (Next() & 0xFFFFFF00u) | (Next() % 4);
        const U mode = ModeArg();
        const U cache = Next() % 2 ? mode : Next() % 2 ? Next() : mode ^ (1u << (Next() % 32));
        RandomCommon();
        PutLong(At(kShadeCache), cache);
        Capture(g_start);

        g_theirs.Clear();
        d3d_fuzz::g_log = &g_theirs;
        if (which) blend(code, mode);
        else shade(mode);
        Capture(g_theirs_state);

        Restore(g_start);
        g_ours.Clear();
        d3d_fuzz::g_log = &g_ours;
        if (which) D3d_SetBlend(code, mode);
        else D3d_SetShadeMode(mode);
        d3d_fuzz::g_log = nullptr;

        char why[200] = "";
        if (!Same(0, 0, why)) {
            if (bad < 4)
                bof3::Log("shadow      d3d_draw MISMATCH: %s(%X, %X) cache %X: %s",
                          which ? "D3d_SetBlend" : "D3d_SetShadeMode", (unsigned)code, (unsigned)mode,
                          (unsigned)cache, why);
            ++bad;
        }
    }
    return bad;
}

// --- D3d_BindTexture -----------------------------------------------------------

struct BindCover {
    unsigned built, refreshed, hit;
};

unsigned FuzzBind(BindFn theirs, unsigned rounds, BindCover& cover) {
    unsigned bad = 0;
    for (unsigned r = 0; r < rounds; ++r) {
        g_round = 0x300000u + r;
        const U tpage = Next() % 2 ? Next() : Pick(kWords, 16) | (Next() % 4 ? 0 : Next() & 0xFFFF0000u);
        const U clut = Next() % 2 ? Next() & 0x7FFF : Next();
        // Every entry of the tpage's page: states 0..3, random textures.
        const U page = tpage & 0x1F;
        for (U s = 0; s < 32; ++s) {
            unsigned char* e = At(kPageCache + (page * 32 + s) * 0x18);
            for (U k = 0; k < 0x18; ++k) e[k] = static_cast<unsigned char>(Next());
            e[0] = static_cast<unsigned char>(Next() % 2 ? 2 : Next() % 4);
        }
        Capture(g_start);

        g_theirs.Clear();
        d3d_fuzz::g_log = &g_theirs;
        const long ret_theirs = theirs(tpage, clut);
        Capture(g_theirs_state);

        Restore(g_start);
        g_ours.Clear();
        d3d_fuzz::g_log = &g_ours;
        const long ret_ours = D3d_BindTexture(tpage, clut);
        d3d_fuzz::g_log = nullptr;

        if (g_theirs.n > 1 && g_theirs.calls[2].what == 8) ++cover.built;
        else if (g_theirs.n > 2 && g_theirs.calls[2].what == 9) ++cover.refreshed;
        else ++cover.hit;

        char why[200] = "";
        if (!Same(ret_ours, ret_theirs, why)) {
            if (bad < 4)
                bof3::Log("shadow      d3d_draw MISMATCH: D3d_BindTexture(%X, %X) round %u: %s", (unsigned)tpage,
                          (unsigned)clut, r, why);
            ++bad;
        }
    }
    return bad;
}

// --- D3d_CellTexture -----------------------------------------------------------

struct CellCover {
    unsigned empty_built, hit, refreshed, stale_in_use, victim, d27;
};

unsigned FuzzCell(CellFn theirs, unsigned rounds, CellCover& cover) {
    unsigned bad = 0;
    const U counts[] = {0, 1, 2, 3, 4, 8, 16, 0x7F, 0x80, 0x81, 0x84};
    const U counters[] = {0, 1, 2, 0xFFFE, 0xFFFF, 0x7FFF, 0x8000};
    for (unsigned r = 0; r < rounds; ++r) {
        g_round = 0x400000u + r;
        const U first = Next() % 0x300;
        const U count = Next() % 2 ? Pick(counts, 11) : Next() % (kCellCountMax + 1);
        U clut = Next() % 4 ? Next() & 0x7FFF : Pick(kWords, 16);
        // wide, but so that a wrong 16-bit compare reads Gfx_ClutRows inside the image, not a fault
        if (Next() % 16 == 0) clut |= 0x10000u << (Next() % 4);
        const U flags = Next() % 2 ? Next() & 0xFFFF : Next();

        for (U i = 0; i < kCellsBytes; ++i) At(kCells)[i] = static_cast<unsigned char>(Next());
        for (U i = 0; i < kCellRegionBytes; ++i) At(kCellCache)[i] = static_cast<unsigned char>(Next());
        U sum = 0;
        for (U k = 0; k < count * 8; k += 4) sum += GetLong(At(kCells + first * 8 + k));
        const U row = (clut >> 6) < 512 ? (clut >> 6) : 0;
        PutLong(At(kClutRows + row * 8), Next() % 2 ? Next() : Next() % 4);
        const U generation = GetLong(At(kClutRows + ((clut >> 6) & 0x1FF) * 8));
        // 0: all in use (D27); 1: none empty; 2: none empty, no key (a victim); 3: as 2 with every
        // free entry at the counter ceiling 0xFFFF (D27 again); else mixed
        const U pattern = Next() % 7;
        for (U i = 0; i < kCellEntries; ++i) {
            unsigned char* e = At(kCellCache + i * kCellEntry);
            if (pattern > 3 && Next() % 24 == 0) PutLong(e, 0);
            else if (GetLong(e) == 0) PutLong(e, 1);
            U in_use = pattern == 0 ? (Next() % 2 ? 1 : Next() | 1) : (Next() % 3 == 0 ? 0 : Next() & 0xFFFF);
            if (pattern == 0 && (in_use & 0xFFFF) == 0) in_use = 1;
            PutWord(e + 0x14, in_use);
            PutWord(e + 0x16, pattern == 3 && GetWord(e + 0x14) == 0 ? 0xFFFF : Next() % 2 ? Pick(counters, 7) : Next() % 8);
            if (pattern != 2 && pattern != 3 && Next() % 6 == 0) {   // a key match, the sum right or wrong, the generation fresh or stale
                PutWord(e + 0x10, Next() % 8 ? clut : clut ^ 1);
                PutWord(e + 0x12, Next() % 8 ? count : count + 1);
                PutLong(e + 0x18, Next() % 4 ? sum : sum + 1);
                PutLong(e + 0x1C, Next() % 2 ? generation : generation ^ 0x10);
            }
        }
        At(kRenderFlags)[0] = static_cast<unsigned char>(Next());
        Capture(g_start);

        g_theirs.Clear();
        d3d_fuzz::g_log = &g_theirs;
        const int ret_theirs = theirs(first, count, clut, flags);
        Capture(g_theirs_state);

        Restore(g_start);
        g_ours.Clear();
        d3d_fuzz::g_log = &g_ours;
        const int ret_ours = D3d_CellTexture(first, count, clut, flags);
        d3d_fuzz::g_log = nullptr;

        const d3d_fuzz::Call* c0 = g_theirs.n ? &g_theirs.calls[0] : nullptr;
        if (c0 && c0->what == 10 && c0->a[0] == count && ret_theirs == static_cast<int>(count) &&
            (pattern == 0 || pattern == 3 || count >= kCellEntries))
            ++cover.d27;
        else if (c0 && c0->what == 10 && GetLong(g_start.bytes + (kStateBytes - kCellRegionBytes) +
                                                  c0->a[0] * kCellEntry) == 0)
            ++cover.empty_built;
        else if (c0 && c0->what == 10) ++cover.victim;
        else if (c0 && c0->what == 11) ++cover.refreshed;
        else ++cover.hit;
        if (ret_theirs >= 0 && ret_theirs < static_cast<int>(kCellEntries) && (!c0 || c0->what != 10) &&
            (!c0 || c0->what != 11)) {
            const unsigned char* e = g_start.bytes + (kStateBytes - kCellRegionBytes) + ret_theirs * kCellEntry;
            if (GetLong(e + 0x1C) != generation && GetWord(e + 0x14) != 0) ++cover.stale_in_use;
        }

        char why[200] = "";
        if (!Same(ret_ours, ret_theirs, why)) {
            if (bad < 4)
                bof3::Log("shadow      d3d_draw MISMATCH: D3d_CellTexture(%X, %X, %X, %X) round %u: %s",
                          (unsigned)first, (unsigned)count, (unsigned)clut, (unsigned)flags, r, why);
            ++bad;
        }
    }
    return bad;
}

}  // namespace

void SelfTest() {
    // The copies, before anything of ours is injected.
    const auto colour = reinterpret_cast<ColorFn>(CloneWithTable("D3d_PrimColor", 0x59FBA0, 0xF4, 0x23, 0xE4, 4));
    const auto blend = reinterpret_cast<BlendFn>(CloneWithTable("D3d_SetBlend", 0x59FCA0, 0xDC, 0x21, 0xCC, 4));
    const auto shade = Clone<ShadeFn>("D3d_SetShadeMode", 0x59FD80, 0x24, nullptr, 0);
    const bof3::CloneCall bind_calls[] = {
        {0x1A, P(StubFind), 0x5A0830},
        {0x37, P(StubBuild), 0x5A0080},
        {0x6E, P(StubRefresh), 0x5A0510},
    };
    const auto bind = Clone<BindFn>("D3d_BindTexture", 0x59FFE0, 0x97, bind_calls, 3);
    const bof3::CloneCall cell_calls[] = {
        {0xB7, P(StubCellRefresh), 0x5A37D0},
        {0xD3, P(StubCellBuild), 0x5A32B0},
        {0xF9, P(StubCellBuild), 0x5A32B0},
    };
    const auto cell = Clone<CellFn>("D3d_CellTexture", 0x5A3160, 0x144, cell_calls, 3);
    DrawFn draws[sizeof g_handlers / sizeof g_handlers[0]];
    for (unsigned i = 0; i < sizeof g_handlers / sizeof g_handlers[0]; ++i) {
        const Handler& h = g_handlers[i];
        draws[i] = Clone<DrawFn>(h.name, h.original, h.size, h.calls, h.n_calls);
    }

    Capture(g_saved);
    const Callees saved_callees = g;
    g = kStandIns;
    d3d_fuzz::Seed(0x84848484u);

    unsigned bad = 0;
    constexpr unsigned kColourRounds = 200000, kStateRounds = 20000, kBindRounds = 20000, kCellRounds = 40000,
                       kDrawRounds = 20000;
    BindCover bind_cover = {};
    CellCover cell_cover = {};
    {
        d3d_fuzz::DeviceSwap swap;
        bad += FuzzColour(colour, kColourRounds);
        bad += FuzzBlendShade(blend, shade, kStateRounds);
        bad += FuzzBind(bind, kBindRounds, bind_cover);
        bad += FuzzCell(cell, kCellRounds, cell_cover);
        for (unsigned i = 0; i < sizeof g_handlers / sizeof g_handlers[0]; ++i)
            bad += FuzzHandler(g_handlers[i], draws[i], kDrawRounds);
    }

    g = saved_callees;
    Restore(g_saved);

    bof3::Log("shadow      d3d_draw self-test: D3d_PrimColor %u rounds, SetBlend / SetShadeMode %u, "
              "D3d_BindTexture %u (built %u, refreshed %u, hit %u), D3d_CellTexture %u (empty built %u, hit %u, "
              "refreshed %u, stale but in use %u, victim built %u, D27 %u), six handlers %u each under three "
              "control words",
              kColourRounds, kStateRounds, kBindRounds, bind_cover.built, bind_cover.refreshed, bind_cover.hit,
              kCellRounds, cell_cover.empty_built, cell_cover.hit, cell_cover.refreshed, cell_cover.stale_in_use,
              cell_cover.victim, cell_cover.d27, kDrawRounds);
    if (bad) bof3::Fatal("the Direct3D draw differs from the original in %u self-test rounds", bad);
}

}  // namespace d3d_draw
