// BOF3X_SHADOW=glyph_draw: a differential fuzz of the glyph handler and its
// texture lookup against byte-copies of Capcom's, once at start-up, on the
// vertex-block harness (d3d_fuzz.h). docs/glyph-draw.md section 5.
//
// D3d_DrawGlyph: a random primitive, random scales, a random vertex block;
// Capcom's copy, then from the same state ours - once with the texel inset 0
// (Capcom's arithmetic: everything compared byte for byte) and once with 0.5
// (DIV-0025: everything byte for byte except each tu and tv, which must be
// the copy's plus exactly 1/64). Its five callees are recording stand-ins and
// the device is the harness's fake.
//
// Font_GlyphTexture: a random cache with hits, stale hits, near-misses and
// used / free entries seeded, sometimes all 128 used (the D18 overrun), a
// random render flag; Capcom's copy, then ours; the cache, the vertex block the
// overrun reaches, the log and the result compared. Its callee
// Font_BuildGlyphTexture is a recording stand-in that fills the entry the way
// the real one does when it succeeds.
#include <cstdint>
#include <cstring>
#include <initializer_list>

#include "bof3/symbols.gen.h"
#include "game/d3d_fuzz.h"
#include "game/glyph_draw_callees.h"
#include "hook/detour.h"
#include "hook/log.h"

namespace glyph_draw {
namespace {

using U = std::uint32_t;
using d3d_fuzz::Next;
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

// Everything either function reads or writes: the cache, the 8 bytes after it
// and the vertex block (one region, 0x7C9F50..0x7CA9D8), the scales, the CLUT
// rows' generations (rows 0..511), the render flag.
constexpr U kRegion = kCache;
constexpr U kRegionBytes = d3d_fuzz::kVertices + d3d_fuzz::kVertexBytes - kCache;   // 0xA88
constexpr U kClutBytes = 512 * 8;

struct State {
    unsigned char region[kRegionBytes];
    unsigned char clut[kClutBytes];
    unsigned char scales[8];
    unsigned char flags;
};
State g_saved, g_start, g_theirs_state;

void Capture(State& s) {
    std::memcpy(s.region, At(kRegion), kRegionBytes);
    std::memcpy(s.clut, At(kClutRows), kClutBytes);
    std::memcpy(s.scales, At(d3d_fuzz::kScaleY), 8);
    s.flags = At(kRenderFlags)[0];
}
void Restore(const State& s) {
    std::memcpy(At(kRegion), s.region, kRegionBytes);
    std::memcpy(At(kClutRows), s.clut, kClutBytes);
    std::memcpy(At(d3d_fuzz::kScaleY), s.scales, 8);
    At(kRenderFlags)[0] = s.flags;
}

// --- the stand-ins ---------------------------------------------------------
// Each records its arguments whole, and writes what the real callee writes
// where its caller reads it again: the colour pair, the cache entry, and -
// for the texture lookup, sometimes - the in-use word the real one's overrun
// puts on vertex 0's sz, and a new code byte in the primitive (the handler
// re-reads +7 after the lookup).

U g_round;
unsigned char* g_prim;   // the primitive of this round, for the lookup's stand-in to disturb

U Mix(U salt) {
    U h = (g_round * 0x9E3779B1u) ^ (salt * 0x85EBCA6Bu) ^ (d3d_fuzz::g_log->n * 0xC2B2AE35u);
    h ^= h >> 16;
    h *= 0x7FEB352Du;
    h ^= h >> 15;
    return h;
}

void __cdecl StubPrimColor(unsigned r, unsigned gg, unsigned b, unsigned code, unsigned mode,
                           unsigned long* diffuse, unsigned long* specular) {
    Record(1, r, gg, b, code, mode, specular != nullptr, diffuse != nullptr);
    *diffuse = Mix(1);
    if (specular) *specular = Mix(2);
}
int __cdecl StubGlyphTexture(unsigned glyph, unsigned clut) {
    Record(2, glyph, clut);
    const U h = Mix(3);
    if (h % 5 == 0) PutWord(At(0x7CA960), 1);                       // the D18 overrun's store
    if (h % 7 == 0 && g_prim) g_prim[7] = static_cast<unsigned char>(h >> 8);
    return static_cast<int>(h >> 20);
}
void __cdecl StubRetOnly(unsigned a) { Record(3, a); }
void __cdecl StubSetBlend(unsigned code, unsigned mode) { Record(4, code, mode); }
void __cdecl StubSetShade(unsigned mode) { Record(5, mode); }
void __cdecl StubBuild(int slot, unsigned glyph, unsigned clut) {
    Record(6, static_cast<U>(slot), glyph, clut);
    const U h = Mix(4);
    if (h % 4 != 0 && slot >= 0 && slot < static_cast<int>(kEntries)) {
        // The real one's success path: the glyph and CLUT words, the row's
        // generation, a surface and a texture.
        unsigned char* e = At(kCache + static_cast<U>(slot) * kEntry);
        PutWord(e, glyph);
        PutWord(e + 2, clut);
        PutLong(e + 4, GetLong(At(kClutRows + ((clut & 0xFFFFu) >> 6) * 8)));
        PutLong(e + 8, Mix(5));
        PutLong(e + 0xC, Mix(6));
    }
}

const Callees kStandIns = {StubPrimColor, StubGlyphTexture, StubRetOnly, StubSetBlend, StubSetShade, StubBuild};

// --- the copies ------------------------------------------------------------
// Offsets of every E8 in each body, from the disassembly (docs/glyph-draw.md
// sections 2 and 3); every other transfer is internal or through the device.

using DrawFn = long(__cdecl*)(const unsigned char*);
using LookupFn = int(__cdecl*)(unsigned, unsigned);

DrawFn CloneDraw() {
    const bof3::CloneCall calls[] = {
        {0x2C, reinterpret_cast<const void*>(&StubPrimColor), 0x59FBA0},
        {0x25B, reinterpret_cast<const void*>(&StubGlyphTexture), 0x5A2BC0},
        {0x262, reinterpret_cast<const void*>(&StubRetOnly), kRetOnly},
        {0x269, reinterpret_cast<const void*>(&StubRetOnly), kRetOnly},
        {0x276, reinterpret_cast<const void*>(&StubSetBlend), 0x59FCA0},
        {0x27D, reinterpret_cast<const void*>(&StubSetShade), 0x59FD80},
    };
    return reinterpret_cast<DrawFn>(bof3::CloneOriginal("D3d_DrawGlyph", bof3::addr::D3d_DrawGlyph, 0x2B4, calls,
                                                        sizeof calls / sizeof calls[0]));
}
LookupFn CloneLookup() {
    const bof3::CloneCall calls[] = {
        {0x73, reinterpret_cast<const void*>(&StubBuild), 0x5A2CA0},
        {0x8F, reinterpret_cast<const void*>(&StubBuild), 0x5A2CA0},
    };
    return reinterpret_cast<LookupFn>(bof3::CloneOriginal("Font_GlyphTexture", bof3::addr::Font_GlyphTexture, 0xD7,
                                                          calls, sizeof calls / sizeof calls[0]));
}

// --- D3d_DrawGlyph -----------------------------------------------------------

constexpr U kPrimBytes = 0x28;

const U kColours[] = {0x00, 0x01, 0x7F, 0x80, 0x81, 0xFF};
const U kCodes[] = {0x6C, 0x6D, 0x6E, 0x6F, 0x2C, 0x64};
const U kCoords[] = {0x0000, 0x0001, 0xFFFF, 0x7FFF, 0x8000, 0x00A0, 0xFF60, 0x0140};
const U kTexels[] = {0x00, 0x01, 0x0C, 0x18, 0x7F, 0x80, 0xFF};
const U kWords[] = {0x0000, 0x0001, 0x7FFF, 0x8000, 0xFFFF, 0x0A00, 0x7FC0};
const float kScales[] = {2.0f, 1.0f, 1.5f, 3.0f, 0.5f, -2.0f, 2.25f};

void RandomPrim(unsigned char* p) {
    for (U i = 0; i < kPrimBytes; ++i) p[i] = static_cast<unsigned char>(Next());
    for (U i = 4; i < 7; ++i)
        if (Next() % 2) p[i] = static_cast<unsigned char>(d3d_fuzz::Pick(kColours, 6));
    if (Next() % 4) p[7] = static_cast<unsigned char>(d3d_fuzz::Pick(kCodes, 6));
    for (U c = 0; c < 4; ++c) {
        if (Next() % 2) PutWord(p + 8 + 8 * c, d3d_fuzz::Pick(kCoords, 8));
        if (Next() % 2) PutWord(p + 0xA + 8 * c, d3d_fuzz::Pick(kCoords, 8));
        if (Next() % 2) p[0xC + 8 * c] = static_cast<unsigned char>(d3d_fuzz::Pick(kTexels, 7));
        if (Next() % 2) p[0xD + 8 * c] = static_cast<unsigned char>(d3d_fuzz::Pick(kTexels, 7));
    }
    if (Next() % 2) PutWord(p + 0xE, d3d_fuzz::Pick(kWords, 7));
    if (Next() % 2) PutWord(p + 0x16, d3d_fuzz::Pick(kWords, 7));
}

void RandomScales() {
    for (U a : {d3d_fuzz::kScaleX, d3d_fuzz::kScaleY}) {
        float f;
        if (Next() % 4) {
            f = kScales[Next() % (sizeof kScales / sizeof kScales[0])];
        } else {
            f = static_cast<float>(Next() % 4096) / 1024.0f;
        }
        std::memcpy(At(a), &f, sizeof f);
    }
}

void RandomVertices() {
    for (U i = 0; i < d3d_fuzz::kVertexBytes; ++i) At(d3d_fuzz::kVertices)[i] = static_cast<unsigned char>(Next());
}

// DIV-0025's rule for one block of four vertices: every byte the original's,
// but each tu and tv its value plus 1/64 exactly.
bool SameWithInset(const unsigned char* ours, const unsigned char* theirs) {
    for (U i = 0; i < 4; ++i) {
        const unsigned char* a = ours + i * 0x20;
        const unsigned char* b = theirs + i * 0x20;
        if (std::memcmp(a, b, 0x18) != 0) return false;
        for (U k = 0x18; k < 0x20; k += 4) {
            float fa, fb;
            std::memcpy(&fa, a + k, 4);
            std::memcpy(&fb, b + k, 4);
            const float want = fb + 1.0f / 64.0f;
            if (std::memcmp(&fa, &want, 4) != 0) return false;
        }
    }
    return true;
}
bool SameBytes(const unsigned char* ours, const unsigned char* theirs) {
    return std::memcmp(ours, theirs, d3d_fuzz::kSnapBytes) == 0;
}

d3d_fuzz::Log g_theirs, g_ours;

unsigned FuzzDraw(DrawFn theirs, unsigned rounds, bool inset) {
    unsigned bad = 0;
    unsigned char prim[kPrimBytes], prim_start[kPrimBytes];
    for (unsigned r = 0; r < rounds; ++r) {
        g_round = r * 2 + (inset ? 1u : 0u);
        RandomPrim(prim_start);
        RandomScales();
        RandomVertices();
        Capture(g_start);

        std::memcpy(prim, prim_start, kPrimBytes);
        g_prim = prim;
        g_theirs.Clear();
        d3d_fuzz::g_log = &g_theirs;
        const long ret_theirs = theirs(prim);
        unsigned char v_theirs[d3d_fuzz::kVertexBytes];
        std::memcpy(v_theirs, At(d3d_fuzz::kVertices), sizeof v_theirs);
        Capture(g_theirs_state);

        Restore(g_start);
        std::memcpy(prim, prim_start, kPrimBytes);
        g_ours.Clear();
        d3d_fuzz::g_log = &g_ours;
        g_texel_inset = inset ? 0.5f : 0.0f;
        const long ret_ours = D3d_DrawGlyph(prim);
        g_texel_inset = 0.0f;
        d3d_fuzz::g_log = nullptr;
        g_prim = nullptr;

        char why[200] = "";
        const d3d_fuzz::SnapCompare rule = inset ? SameWithInset : SameBytes;
        bool same = d3d_fuzz::SameLog(g_ours, g_theirs, why, rule);
        if (same && ret_ours != ret_theirs) {
            same = false;
            std::strcpy(why, "the return value");
        }
        if (same && !rule(At(d3d_fuzz::kVertices), v_theirs)) {
            same = false;
            std::strcpy(why, "the vertex block");
        }
        // Everything outside the vertex block, byte for byte.
        if (same && std::memcmp(At(kRegion), g_theirs_state.region, d3d_fuzz::kVertices - kRegion) != 0) {
            same = false;
            std::strcpy(why, "the cache");
        }
        if (!same) {
            if (bad < 4)
                bof3::Log("shadow      glyph_draw MISMATCH: D3d_DrawGlyph%s round %u: %s", inset ? " (inset)" : "", r,
                          why);
            ++bad;
        }
    }
    return bad;
}

// --- Font_GlyphTexture -------------------------------------------------------

struct LookupCover {
    unsigned hit, built_software, built_scene, overrun, stale, wide;
};

unsigned FuzzLookup(LookupFn theirs, unsigned rounds, LookupCover& cover) {
    unsigned bad = 0;
    for (unsigned r = 0; r < rounds; ++r) {
        g_round = 0x100000u + r;
        // Arguments: a glyph and a CLUT, now and then with the high half set.
        U glyph = d3d_fuzz::Pick(kWords, 7);
        if (Next() % 2) glyph = Next() & 0xFFFF;
        U clut = Next() % 3 ? (Next() & 0x7FFF) : d3d_fuzz::Pick(kWords, 7);
        if (Next() % 16 == 0) glyph |= 0x10000u << (Next() % 16);
        if (Next() % 16 == 0) clut |= 0x10000u << (Next() % 16);

        // The cache: random bytes, then the in-use words by one of four
        // patterns, then some entries made hits or near-misses of this call.
        for (U i = 0; i < kRegionBytes; ++i) At(kRegion)[i] = static_cast<unsigned char>(Next());
        const U pattern = Next() % 4;
        for (U i = 0; i < kEntries; ++i) {
            unsigned char* e = At(kCache + i * kEntry);
            U used;
            switch (pattern) {
            case 0: used = 1; break;                                           // all used: the overrun
            case 1: used = Next() % 8 == 0 ? 0 : 1; break;                     // a few free
            case 2: used = Next() % 2; break;
            default: used = Next() % 3 == 0 ? 0 : (Next() % 2 ? 0x100 : Next() & 0xFFFF); break;
            }
            if (pattern == 0 && used == 0) used = 1;
            PutWord(e + 0x10, used);
        }
        const U row = (clut >> 6);
        const bool row_ok = row < 1024;
        if ((clut >> 6) < 512) PutLong(At(kClutRows + row * 8), Next() % 2 ? Next() : Next() % 4);
        const U generation = row_ok ? GetLong(At(kClutRows + row * 8)) : 0;
        const U seeds = Next() % 4;
        for (U k = 0; k < seeds; ++k) {
            unsigned char* e = At(kCache + (Next() % kEntries) * kEntry);
            PutWord(e, Next() % 8 ? glyph : glyph + 1);
            PutWord(e + 2, Next() % 8 ? clut : clut ^ 0x40);
            PutLong(e + 4, Next() % 4 ? generation : generation + 1);
        }
        const U flags = Next() & 0xFF;
        At(kRenderFlags)[0] = static_cast<unsigned char>(Next() % 2 ? (flags | 1) : (flags & ~1u));
        Capture(g_start);

        g_theirs.Clear();
        d3d_fuzz::g_log = &g_theirs;
        const int ret_theirs = theirs(glyph, clut);
        Capture(g_theirs_state);

        Restore(g_start);
        g_ours.Clear();
        d3d_fuzz::g_log = &g_ours;
        const int ret_ours = Font_GlyphTexture(glyph, clut);
        d3d_fuzz::g_log = nullptr;

        if (ret_theirs == static_cast<int>(kEntries)) ++cover.overrun;
        else if (g_theirs.n > 0 && g_theirs.calls[0].what == 6) ++cover.built_software;
        else if (g_theirs.n > 1 && g_theirs.calls[1].what == 6) ++cover.built_scene;
        else ++cover.hit;
        if (glyph > 0xFFFF || clut > 0xFFFF) ++cover.wide;
        for (U k = 0; k < kEntries; ++k) {
            const unsigned char* e = g_start.region + k * kEntry;
            if (std::memcmp(e, &glyph, 2) == 0 && std::memcmp(e + 2, &clut, 2) == 0 && GetLong(e + 4) != generation) {
                ++cover.stale;
                break;
            }
        }

        char why[200] = "";
        bool same = d3d_fuzz::SameLog(g_ours, g_theirs, why);
        if (same && ret_ours != ret_theirs) {
            same = false;
            std::strcpy(why, "the return value");
        }
        if (same && std::memcmp(At(kRegion), g_theirs_state.region, kRegionBytes) != 0) {
            same = false;
            std::strcpy(why, "the cache or the vertex block");
        }
        if (same && At(kRenderFlags)[0] != g_theirs_state.flags) {
            same = false;
            std::strcpy(why, "the render flag");
        }
        if (!same) {
            if (bad < 4)
                bof3::Log("shadow      glyph_draw MISMATCH: Font_GlyphTexture round %u (glyph %X clut %X): %s", r,
                          (unsigned)glyph, (unsigned)clut, why);
            ++bad;
        }
    }
    return bad;
}

}  // namespace

void SelfTest() {
    const DrawFn draw = CloneDraw();
    const LookupFn lookup = CloneLookup();
    if (!draw || !lookup) bof3::Fatal("glyph_draw: CloneOriginal returned null");

    Capture(g_saved);
    const Callees saved_callees = g;
    g = kStandIns;
    d3d_fuzz::Seed(0x6C6C6C6Cu);

    unsigned bad = 0;
    constexpr unsigned kDrawRounds = 20000, kLookupRounds = 40000;
    LookupCover cover = {};
    {
        d3d_fuzz::DeviceSwap swap;
        bad += FuzzDraw(draw, kDrawRounds, false);
        bad += FuzzDraw(draw, kDrawRounds, true);
        bad += FuzzLookup(lookup, kLookupRounds, cover);
    }

    g = saved_callees;
    g_texel_inset = 0.0f;
    Restore(g_saved);

    bof3::Log("shadow      glyph_draw self-test: D3d_DrawGlyph %u rounds exact + %u with the DIV-0025 inset, "
              "Font_GlyphTexture %u rounds (hit %u, built software %u, built in scene %u, overrun %u, stale hit "
              "seeded %u, wide argument %u)",
              kDrawRounds, kDrawRounds, kLookupRounds, cover.hit, cover.built_software, cover.built_scene,
              cover.overrun, cover.stale, cover.wide);
    if (bad) bof3::Fatal("the glyph draw differs from the original in %u self-test rounds", bad);
}

}  // namespace glyph_draw
