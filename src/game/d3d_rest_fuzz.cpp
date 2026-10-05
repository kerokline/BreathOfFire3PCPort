// BOF3X_SHADOW=d3d_rest: a differential fuzz of the renderer's live remainder
// against byte-copies of Capcom's, once at start-up (docs/d3d-rest.md section
// 7), on the vertex-block harness (d3d_fuzz.h) for the handlers and
// D3d_SetAlphaModulate, on fake DirectDraw surfaces of this file's own for
// D3d_AfterDraw, and on plain memory for Gfx_StoreImage.
//
// Every copy has each of its calls re-aimed at a recording stand-in, the same
// stand-ins ours is put on through d3d_rest::g; POLY_FT3's copy calls a copy
// of D3d_FlattenFT3, which calls a copy of D3d_PageTexel4, so the chain is
// compared whole. The CRT's _ftol is left where the copies call it (a pure
// function of the x87 state; ours inlines the same sequence). Per round:
// random state with its boundaries seeded; Capcom's copy, then from the same
// state ours; the log of calls, the vertices each DrawPrimitive was handed,
// every byte of state either could touch and the return value compared. The
// x87 functions run under a control word picked per round from 0x027F (the
// game's, measured), 0x007F and 0x037F.
//
// The stand-ins write what the real callees write where the caller reads it
// again (the colour pair, a palette, a surface and its description) - and,
// a quarter of the time, a byte of what the caller reads after the call: so
// every read and store is ordered against every call, and an order changed
// shows.
#include <windows.h>

#include <cstdint>
#include <cstdio>
#include <cstring>
#include <initializer_list>
#include <utility>

#include "bof3/symbols.gen.h"
#include "game/d3d_fuzz.h"
#include "game/d3d_rest_callees.h"
#include "hook/detour.h"
#include "hook/log.h"

// Ours, declared by symbols.gen.h; the fuzz calls them directly.
namespace d3d_rest {
namespace {

using d3d_fuzz::Next;
using d3d_fuzz::Pick;
using d3d_fuzz::Record;

constexpr U kFtol = 0x5B9550;   // the CRT's _ftol (unnamed in symbols.toml): the copies call it as the original does

unsigned char* At(U address) { return reinterpret_cast<unsigned char*>(static_cast<std::uintptr_t>(address)); }
U Addr(const void* p) { return static_cast<U>(reinterpret_cast<std::uintptr_t>(p)); }
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
template <typename F> const void* P(F* f) { return reinterpret_cast<const void*>(f); }

unsigned short GetControlWord() {
    unsigned short cw;
    __asm__ volatile("fnstcw %0" : "=m"(cw));
    return cw;
}
void SetControlWord(unsigned short cw) { __asm__ volatile("fldcw %0" : : "m"(cw)); }
const unsigned short kControlWords[] = {0x027F, 0x007F, 0x037F};

// --- the state -------------------------------------------------------------

unsigned char g_palette[0x80];   // what the Gfx_ClutPixels stand-in hands out
constexpr U kVramBytes = 0x100000;
unsigned char g_vram_saved[kVramBytes];

struct Region {
    U address, bytes;
};
const Region kRegions[] = {
    {kVertices, 0x80}, {kScaleY, 8}, {kTexCoords, 0x400}, {kDrawTpage, 4}, {kPixelFormat, 0x20},
    {kAlphaOpCache, 4}, {kCaptureReady, 0x1C}, {kRenderFlags, 1}, {kBackBuffer, 4}, {kScreenBpp, 1},
};
U g_palette_at;   // Addr(g_palette), as a region of its own
constexpr U kStateBytes = 0x80 + 8 + 0x400 + 4 + 0x20 + 4 + 0x1C + 1 + 4 + 1 + sizeof g_palette;
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
    std::memcpy(s.bytes + at, g_palette, sizeof g_palette);
}
void Restore(const State& s) {
    U at = 0;
    for (const Region& r : kRegions) {
        std::memcpy(At(r.address), s.bytes + at, r.bytes);
        at += r.bytes;
    }
    std::memcpy(g_palette, s.bytes + at, sizeof g_palette);
}
bool FirstDifference(const State& a, const State& b, U* where) {
    for (U i = 0; i < kStateBytes; ++i)
        if (a.bytes[i] != b.bytes[i]) {
            U at = 0;
            for (const Region& r : kRegions) {
                if (i < at + r.bytes) {
                    *where = r.address + (i - at);
                    return true;
                }
                at += r.bytes;
            }
            *where = g_palette_at + (i - at);
            return true;
        }
    return false;
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
    switch ((h >> 2) % 8) {
    case 0:
    case 1:
        if (g_prim) g_prim[(h >> 8) % g_prim_bytes] = value;
        break;
    case 2: At(kVertices)[(h >> 8) % 0x80] = value; break;
    case 3: At(kScaleY)[(h >> 8) % 8] = value; break;
    case 4: At(kDrawTpage)[(h >> 8) % 2] = value; break;
    case 5: At(kTexCoords + kTexels[(h >> 8) % 6] * 4u)[(h >> 16) % 4] = value; break;
    case 6: {   // Gfx_PixelFormat, its bytes-per-texel byte (+3) kept to 2 or 4
        const U at = (h >> 8) % 0x1C;
        At(kPixelFormat)[at] = at == 3 ? static_cast<unsigned char>(value & 2 ? 4 : 2) : value;
        break;
    }
    default: g_palette[(h >> 8) % sizeof g_palette] = value; break;
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
// Gfx_ClutPixels: a pointer into this file's palette (an offset seeded), the
// palette disturbed now and then before the caller indexes it.
void* __cdecl StubClutPixels(unsigned clut) {
    Record(6, clut);
    Disturb(9);
    return g_palette + (Mix(10) % 8) * 4;
}

// --- the copies --------------------------------------------------------------

template <typename Fn> Fn Clone(const char* name, U original, U size, const bof3::CloneCall* calls, int n) {
    void* code = bof3::CloneOriginal(name, original, size, calls, n);
    if (!code) bof3::Fatal("d3d_rest: CloneOriginal(%s) returned null", name);
    return reinterpret_cast<Fn>(code);
}

using DrawFn = long(__cdecl*)(unsigned char*);
using AlphaFn = void(__cdecl*)(unsigned);
using AfterFn = void(__cdecl*)();
using StoreFn = int(__cdecl*)(const short*, void*);
using FlattenFn = void(__cdecl*)(unsigned char*, U);
using TexelFn = U(__cdecl*)(U, U, U);

// The handlers: every E8 of each body, from the disassembly (docs/d3d-rest.md
// section 2); every other transfer is internal or through the device. POLY_FT3's
// first (+0x1B, D3d_FlattenFT3) is filled in at run time with the helper's copy.
struct Handler {
    const char* name;
    U original, size;
    DrawFn ours;
    U prim_bytes;
    bof3::CloneCall calls[10];
    int n_calls;
    unsigned char floats[12];
    unsigned char n_floats;
    unsigned char texels[6];
    unsigned char n_texels;
    unsigned char words[3];
    unsigned char n_words;
    unsigned char colours[4];   // r, g, b triples
    unsigned char n_colours;
};

long __cdecl OursF3(unsigned char* p) { return D3d_DrawPolyF3(p); }
long __cdecl OursFT3(unsigned char* p) { return D3d_DrawPolyFT3(p); }
long __cdecl OursGT3(unsigned char* p) { return D3d_DrawPolyGT3(p); }
long __cdecl OursLineG4(unsigned char* p) { return D3d_DrawLineG4(p); }
long __cdecl OursTile1(unsigned char* p) { return D3d_DrawTile1(p); }

#define CALL(off, stub, addr) {off, P(stub), addr}
const U kPrim = bof3::addr::D3d_PrimColor, kBindAt = bof3::addr::D3d_BindTexture, kBlend = bof3::addr::D3d_SetBlend,
        kShade = bof3::addr::D3d_SetShadeMode;

Handler g_handlers[] = {
    {"D3d_DrawPolyF3", bof3::addr::D3d_DrawPolyF3, 0x14C, OursF3, 0x2C,
     {CALL(0x31, StubPrimColor, kPrim), CALL(0x101, StubRet, kRetOnly), CALL(0x108, StubRet, kRetOnly),
      CALL(0x120, StubBlend, kBlend), CALL(0x127, StubShade, kShade)},
     5,
     {0x08, 0x0C, 0x10, 0x14, 0x18, 0x1C, 0x20, 0x24, 0x28}, 9,
     {}, 0,
     {}, 0,
     {4}, 1},
    {"D3d_DrawPolyFT3", bof3::addr::D3d_DrawPolyFT3, 0x226, OursFT3, 0x38,
     {CALL(0x1B, StubRet, bof3::addr::D3d_FlattenFT3), CALL(0x51, StubPrimColor, kPrim), CALL(0x1B0, StubBind, kBindAt),
      CALL(0x1D6, StubRet, kRetOnly), CALL(0x1E6, StubRet, kRetOnly), CALL(0x1F8, StubBlend, kBlend),
      CALL(0x1FF, StubShade, kShade)},
     7,
     {0x08, 0x0C, 0x10, 0x18, 0x1C, 0x20, 0x28, 0x2C, 0x30}, 9,
     {0x14, 0x15, 0x24, 0x25, 0x34, 0x35}, 6,
     {0x16, 0x26, 0x0E}, 3,
     {4}, 1},
    {"D3d_DrawPolyGT3", bof3::addr::D3d_DrawPolyGT3, 0x239, OursGT3, 0x40,
     {CALL(0x31, StubPrimColor, kPrim), CALL(0x5F, StubPrimColor, kPrim), CALL(0x8D, StubPrimColor, kPrim),
      CALL(0x1EA, StubBind, kBindAt), CALL(0x1F1, StubRet, kRetOnly), CALL(0x1F8, StubRet, kRetOnly),
      CALL(0x20A, StubBlend, kBlend), CALL(0x211, StubShade, kShade)},
     8,
     {0x08, 0x0C, 0x10, 0x1C, 0x20, 0x24, 0x30, 0x34, 0x38}, 9,
     {0x14, 0x15, 0x28, 0x29, 0x3C, 0x3D}, 6,
     {0x16, 0x2A}, 2,
     {4, 0x18, 0x2C}, 3},
    {"D3d_DrawLineG4", bof3::addr::D3d_DrawLineG4, 0x22F, OursLineG4, 0x44,
     {CALL(0x34, StubPrimColor, kPrim), CALL(0x65, StubPrimColor, kPrim), CALL(0x96, StubPrimColor, kPrim),
      CALL(0xCA, StubPrimColor, kPrim), CALL(0x1E3, StubRet, kRetOnly), CALL(0x1EA, StubRet, kRetOnly),
      CALL(0x200, StubBlend, kBlend), CALL(0x207, StubShade, kShade)},
     8,
     {0x08, 0x0C, 0x10, 0x18, 0x1C, 0x20, 0x28, 0x2C, 0x30, 0x38, 0x3C, 0x40}, 12,
     {}, 0,
     {}, 0,
     {4, 0x14, 0x24, 0x34}, 4},
    {"D3d_DrawTile1", bof3::addr::D3d_DrawTile1, 0xD5, OursTile1, 0x14,
     {CALL(0x31, StubPrimColor, kPrim), CALL(0x8C, StubRet, kRetOnly), CALL(0x93, StubRet, kRetOnly),
      CALL(0xA9, StubBlend, kBlend), CALL(0xB0, StubShade, kShade)},
     5,
     {0x08, 0x0C, 0x10}, 3,
     {}, 0,
     {}, 0,
     {4}, 1},
};
#undef CALL
constexpr unsigned kHandlerCount = sizeof g_handlers / sizeof g_handlers[0];

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

void RandomFloat(unsigned char* p) { PutLong(p, Next() % 3 ? Pick(kFloats, sizeof kFloats / 4) : Next()); }

// Gfx_PixelFormat's first record: 1-5-5-5 as the owner's machine had it (the
// unpacking shifts 9, 14, 19 put each 5-bit field at bits 19..23), 5-6-5,
// X-8-8-8 with 32-bit texels, or random - byte +3 2, 4 or anything.
void RandomPixelFormat() {
    unsigned char* f = At(kPixelFormat);
    for (U i = 0; i < 0x20; ++i) f[i] = static_cast<unsigned char>(Next());
    switch (Next() % 5) {
    case 0:
        f[3] = 2;
        PutLong(f + 4, 9), PutLong(f + 8, 14), PutLong(f + 0xC, 19);
        PutLong(f + 0x10, 0x7C00), PutLong(f + 0x14, 0x3E0), PutLong(f + 0x18, 0x1F);
        break;
    case 1:
        f[3] = 2;
        PutLong(f + 4, 8), PutLong(f + 8, 13), PutLong(f + 0xC, 19);
        PutLong(f + 0x10, 0xF800), PutLong(f + 0x14, 0x7E0), PutLong(f + 0x18, 0x1F);
        break;
    case 2:
        f[3] = 4;
        PutLong(f + 4, 0), PutLong(f + 8, 8), PutLong(f + 0xC, 16);
        PutLong(f + 0x10, 0xFF0000), PutLong(f + 0x14, 0xFF00), PutLong(f + 0x18, 0xFF);
        break;
    case 3: f[3] = static_cast<unsigned char>(Next() % 2 ? 2 : 4); break;
    default: break;
    }
    for (U i = 0; i < sizeof g_palette; ++i) g_palette[i] = static_cast<unsigned char>(Next());
    if (Next() % 4 == 0)
        for (U i = 0; i < sizeof g_palette; ++i) g_palette[i] = static_cast<unsigned char>(Next() % 2 ? 0xFF : 0);
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
    RandomPixelFormat();
}

void RandomPrim(const Handler& h, unsigned char* p) {
    for (U i = 0; i < h.prim_bytes; ++i) p[i] = static_cast<unsigned char>(Next());
    for (U c = 0; c < h.n_colours; ++c)
        for (U i = 0; i < 3; ++i)
            if (Next() % 2) p[h.colours[c] + i] = static_cast<unsigned char>(Pick(kColours, 6));
    if (Next() % 2) p[7] = static_cast<unsigned char>((Next() % 2 ? 0x24 : 0x20) | (Next() % 4));
    for (U i = 0; i < h.n_floats; ++i)
        if (Next() % 4) RandomFloat(p + h.floats[i]);
    for (U i = 0; i < h.n_texels; ++i)
        if (Next() % 2) p[h.texels[i]] = kTexels[Next() % 6];
    for (U i = 0; i < h.n_words; ++i)
        if (Next() % 2) PutWord(p + h.words[i], Pick(kWords, 16));
    if (h.original == bof3::addr::D3d_DrawPolyFT3) {
        // One texel - every corner's (u, v) the same - half the time; two of three now and then.
        const U pattern = Next() % 8;
        if (pattern < 4) {
            PutWord(p + 0x24, GetWord(p + 0x14));
            PutWord(p + 0x34, GetWord(p + 0x14));
        } else if (pattern == 4) {
            PutWord(p + 0x24, GetWord(p + 0x14));
        } else if (pattern == 5) {
            PutWord(p + 0x34, GetWord(p + 0x14));
        }
    }
}

d3d_fuzz::Log g_theirs, g_ours;

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

unsigned g_flattened;   // POLY_FT3 rounds that took the one-texel path

unsigned FuzzHandler(const Handler& h, DrawFn theirs, unsigned rounds) {
    unsigned bad = 0;
    unsigned char prim[0x60], prim_start[0x60], prim_theirs[0x60];
    const unsigned short saved_cw = GetControlWord();
    for (unsigned r = 0; r < rounds; ++r) {
        g_round = r + (h.original << 4);
        RandomCommon();
        RandomPrim(h, prim_start);
        const unsigned short cw = kControlWords[Next() % 3];
        Capture(g_start);
        if (h.original == bof3::addr::D3d_DrawPolyFT3 && GetWord(prim_start + 0x14) == GetWord(prim_start + 0x24) &&
            GetWord(prim_start + 0x14) == GetWord(prim_start + 0x34))
            ++g_flattened;

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
            if (bad < 4) bof3::Log("shadow      d3d_rest MISMATCH: %s round %u (cw %04X): %s", h.name, r, cw, why);
            ++bad;
        }
    }
    return bad;
}

// D3d_FlattenFT3 and D3d_PageTexel4 on their own: every argument, the pixel
// format and the palette seeded, under the three control words.
unsigned FuzzFlatten(FlattenFn flatten, TexelFn texel, unsigned rounds) {
    unsigned bad = 0;
    unsigned char prim[0x40], prim_start[0x40], prim_theirs[0x40];
    const unsigned short saved_cw = GetControlWord();
    for (unsigned r = 0; r < rounds; ++r) {
        g_round = 0x500000u + r;
        RandomCommon();
        for (U i = 0; i < sizeof prim_start; ++i) prim_start[i] = static_cast<unsigned char>(Next());
        for (U i = 4; i < 7; ++i)
            if (Next() % 2) prim_start[i] = static_cast<unsigned char>(Pick(kColours, 6));
        if (Next() % 2) PutWord(prim_start + 0x16, Pick(kWords, 16));
        if (Next() % 2) PutWord(prim_start + 0xE, Pick(kWords, 16));
        const bool which = r % 2;   // the texel lookup alone, or the flatten
        const U uv = Next() % 2 ? Next() : (Next() % 2 ? Pick(kWords, 16) : Next() & 0xFFFF);
        const U clut = Next(), page = Next() % 2 ? Next() : Pick(kWords, 16);
        const unsigned short cw = kControlWords[Next() % 3];
        Capture(g_start);

        std::memcpy(prim, prim_start, sizeof prim);
        g_prim = prim;
        g_prim_bytes = 8;
        g_theirs.Clear();
        d3d_fuzz::g_log = &g_theirs;
        SetControlWord(cw);
        U ret_theirs = 0;
        if (which) ret_theirs = texel(clut, page, uv & 0xFFFF);
        else flatten(prim, uv);
        SetControlWord(saved_cw);
        Capture(g_theirs_state);
        std::memcpy(prim_theirs, prim, sizeof prim);

        Restore(g_start);
        std::memcpy(prim, prim_start, sizeof prim);
        g_ours.Clear();
        d3d_fuzz::g_log = &g_ours;
        SetControlWord(cw);
        U ret_ours = 0;
        if (which) ret_ours = D3d_PageTexel4(clut, page, uv & 0xFFFF);
        else D3d_FlattenFT3(prim, uv);
        SetControlWord(saved_cw);
        d3d_fuzz::g_log = nullptr;
        g_prim = nullptr;

        char why[200] = "";
        bool same = Same(static_cast<long>(ret_ours), static_cast<long>(ret_theirs), why);
        if (same && std::memcmp(prim, prim_theirs, sizeof prim) != 0) {
            same = false;
            std::strcpy(why, "the primitive");
        }
        if (!same) {
            if (bad < 4)
                bof3::Log("shadow      d3d_rest MISMATCH: %s round %u (uv %X, cw %04X): %s",
                          which ? "D3d_PageTexel4" : "D3d_FlattenFT3", r, (unsigned)uv, cw, why);
            ++bad;
        }
    }
    return bad;
}

// --- D3d_SetAlphaModulate ----------------------------------------------------

unsigned FuzzAlpha(AlphaFn theirs, unsigned rounds) {
    unsigned bad = 0;
    for (unsigned r = 0; r < rounds; ++r) {
        g_round = 0x600000u + r;
        const U on = Pick(std::initializer_list<U>{0, 1, 0x100, 0x80000000u, 0xFFFFFFFFu}.begin(), 5);
        const U arg = Next() % 3 ? on : Next();
        const U cache = Next() % 3 ? Next() % 2 : (Next() % 2 ? 0xFFFFFFFFu : Next());
        PutLong(At(kAlphaOpCache), cache);
        Capture(g_start);

        g_theirs.Clear();
        d3d_fuzz::g_log = &g_theirs;
        theirs(arg);
        Capture(g_theirs_state);

        Restore(g_start);
        g_ours.Clear();
        d3d_fuzz::g_log = &g_ours;
        D3d_SetAlphaModulate(arg);
        d3d_fuzz::g_log = nullptr;

        char why[200] = "";
        if (!Same(0, 0, why)) {
            if (bad < 4)
                bof3::Log("shadow      d3d_rest MISMATCH: D3d_SetAlphaModulate(%X) cache %X: %s", (unsigned)arg,
                          (unsigned)cache, why);
            ++bad;
        }
    }
    return bad;
}

// --- D3d_AfterDraw -------------------------------------------------------------
// Fake DirectDraw surfaces: two capture surfaces, two back buffers and two
// texture surfaces, so that a surface pointer read at the wrong moment names
// the wrong one. Lock, Unlock and Blt record (0x200 + slot); every other slot
// ends the process naming it. A lock hands out this file's pixels: one capture
// buffer (320 x 240 x 4) and one back buffer large enough for every seeded
// scale and pitch.

constexpr U kPlainBytes = 320 * 240 * 4;
constexpr U kBackBytes = 0x200000;
unsigned char g_plain[kPlainBytes];
unsigned char g_back[kBackBytes];

struct FakeSurface {
    void** vtbl;
    U id;
};
void* g_surface_vtbl[33];
FakeSurface g_surfaces[6] = {{g_surface_vtbl, 1}, {g_surface_vtbl, 2}, {g_surface_vtbl, 3},
                             {g_surface_vtbl, 4}, {g_surface_vtbl, 5}, {g_surface_vtbl, 6}};
FakeSurface* const kPlainA = &g_surfaces[0];
FakeSurface* const kPlainB = &g_surfaces[1];
FakeSurface* const kBackA = &g_surfaces[2];
FakeSurface* const kBackB = &g_surfaces[3];
FakeSurface* const kTexA = &g_surfaces[4];
FakeSurface* const kTexB = &g_surfaces[5];

U Id(const void* p) {
    for (const FakeSurface& s : g_surfaces)
        if (p == &s) return s.id;
    return Addr(p) ? 0xBAD : 0;
}

const U kTestScales[] = {0x3F000000, 0x3F800000, 0x3FC00000, 0x40000000, 0x40000001, 0x40400000, 0x3FA00000,
                         0x3F400000};   // .5 1 1.5 2 2+ulp 3 1.25 .75
const U kPitches[] = {0x280, 0x500, 0xA00, 0xA04, 0x780, 0x284};

// A quarter of the time, something the capture reads after the call: the used
// size words, the screen's bytes per pixel, the scales (seeded values only:
// the back buffer is sized for them), or which surface a global names.
void DisturbAfter(U salt) {
    const U h = Mix(salt ^ 0x27D4EB2Fu);
    if (h % 4) return;
    switch ((h >> 2) % 6) {
    case 0: PutWord(At(kCaptureW + 2 * ((h >> 8) % 2)), (h >> 9) % 3 ? h >> 16 : 0x140); break;
    case 1: At(kScreenBpp)[0] = static_cast<unsigned char>((h >> 8) % 2 ? 2 : 4); break;
    case 2: PutLong(At((h >> 8) % 2 ? kScaleX : kScaleY), kTestScales[(h >> 9) % 8]); break;
    case 3:
        if (GetLong(At(kCaptureSurface)) != 0)
            PutLong(At(kCaptureSurface), Addr((h >> 8) % 2 ? kPlainA : kPlainB));
        break;
    case 4: PutLong(At(kBackBuffer), Addr((h >> 8) % 2 ? kBackA : kBackB)); break;
    default: PutLong(At(kCaptureTexSurface), Addr((h >> 8) % 2 ? kTexA : kTexB)); break;
    }
}

long __stdcall FakeBlt(FakeSurface* self, U dst_rect, FakeSurface* src, U src_rect, U flags, U fx) {
    const unsigned char* d = At(dst_rect);
    const unsigned char* s = At(src_rect);
    Record(0x205, self->id, Id(src), flags, fx, GetLong(d), GetLong(d + 4), GetLong(d + 8));
    Record(0x206, GetLong(d + 0xC), GetLong(s), GetLong(s + 4), GetLong(s + 8), GetLong(s + 0xC));
    DisturbAfter(1);
    return static_cast<long>(Mix(2));
}
// Lock: the pixels (+0x24) always; the pitch (+0x10) for a back buffer three
// times in four (a capture surface's pitch is the caller's to ignore, and a
// description reused from the first lock shows when the second skips it);
// failing one time in eight.
long __stdcall FakeLock(FakeSurface* self, U rect, U desc, U flags, U event) {
    unsigned char* d = At(desc);
    Record(0x219, self->id, rect, flags, event, GetLong(d), GetLong(d + 0x48));
    const U h = Mix(3);
    const bool back = self == kBackA || self == kBackB;
    PutLong(d + 0x24, Addr(back ? g_back : g_plain));
    if (!back || (h >> 4) % 4) PutLong(d + 0x10, kPitches[(h >> 8) % 6]);
    DisturbAfter(4);
    return h % 8 == 0 ? static_cast<long>(Mix(5) | 1) : 0;
}
long __stdcall FakeUnlock(FakeSurface* self, U rect) {
    Record(0x220, self->id, rect);
    DisturbAfter(6);
    return static_cast<long>(Mix(7));
}
template <int S> long __stdcall SurfaceTrap(void*) {
    bof3::Fatal("d3d_rest: the fake surface's slot %d (vtable +0x%X) was called, and nothing records it", S, S * 4);
}
template <int... S> void FillSurfaceTraps(std::integer_sequence<int, S...>) {
    ((g_surface_vtbl[S] = reinterpret_cast<void*>(&SurfaceTrap<S>)), ...);
}

// The four creation helpers' stand-ins: each records, writes what the real one
// writes (a surface, a texture, the two sizes - dwords at 0x7CADF0 and
// 0x7CADF2, which overlap), now and then fails.
void __cdecl StubInitDesc(void* desc) {
    Record(0x10);
    unsigned char* d = static_cast<unsigned char*>(desc);
    for (U i = 0; i < 0x7C; i += 4) PutLong(d + i, Mix(0x11 + i));
    PutLong(d, 0x7C);
    PutLong(d + 0x48, 0x20);
    DisturbAfter(0x12);
}
int __cdecl StubCreatePlain(unsigned w, unsigned h, void** surface, unsigned format) {
    Record(0x11, w, h, Addr(surface), format);
    const U m = Mix(0x13);
    const bool ok = m % 6 != 0;
    if (ok) *surface = (m >> 4) % 2 ? kPlainA : kPlainB;
    else if ((m >> 4) % 2) *surface = nullptr;
    DisturbAfter(0x14);
    return ok ? 1 : 0;
}
void __cdecl StubFit(unsigned w, unsigned h, unsigned* out_w, unsigned* out_h) {
    Record(0x12, w, h, Addr(out_w), Addr(out_h));
    static const U kSizes[] = {0x140, 0x100, 0x200, 0x13F, 0x80, 0x40, 0, 0xFFFF, 0x10100, 0xF0, 0x141, 1};
    const U m = Mix(0x15);
    *out_w = m % 5 == 0 ? Mix(0x16) : kSizes[(m >> 4) % 12];
    *out_h = (m >> 8) % 5 == 0 ? Mix(0x17) : kSizes[(m >> 12) % 12];
    DisturbAfter(0x18);
}
int __cdecl StubCreateTexture(unsigned w, unsigned h, void** surface, void** texture, unsigned format) {
    Record(0x13, w, h, Addr(surface), Addr(texture), format);
    const U m = Mix(0x19);
    const bool ok = m % 6 != 0;
    if (ok) {
        *surface = (m >> 4) % 2 ? kTexA : kTexB;
        *texture = reinterpret_cast<void*>(static_cast<std::uintptr_t>(Mix(0x1A)));
    }
    DisturbAfter(0x1B);
    return ok ? 1 : 0;
}

// The capture buffer as one number: it is 300 KB, compared by a hash of both
// passes' contents rather than kept twice.
U HashPlain() {
    U h = 0x811C9DC5u;
    for (U i = 0; i < kPlainBytes; i += 4) h = (h ^ GetLong(g_plain + i)) * 0x01000193u;
    return h;
}

struct AfterCover {
    unsigned off, first_failed, first_narrow, first_wide, lock_failed, narrow, wide;
};

unsigned FuzzAfterDraw(AfterFn theirs, unsigned rounds, AfterCover& cover) {
    unsigned bad = 0;
    const unsigned short saved_cw = GetControlWord();
    for (unsigned r = 0; r < rounds; ++r) {
        g_round = 0x700000u + r;
        At(kRenderFlags)[0] = static_cast<unsigned char>(Next() % 6 == 0 ? (Next() | 1) : (Next() & ~1u));
        PutLong(At(kScaleX), kTestScales[Next() % 8]);
        PutLong(At(kScaleY), kTestScales[Next() % 8]);
        for (U i = 0; i < 0x1C; ++i) At(kCaptureReady)[i] = static_cast<unsigned char>(Next());
        const bool first = Next() % 2;
        PutLong(At(kCaptureSurface), first ? 0 : Addr(Next() % 2 ? kPlainA : kPlainB));
        PutLong(At(kCaptureTexSurface), Addr(Next() % 2 ? kTexA : kTexB));
        if (Next() % 2) PutWord(At(kCaptureW), 0x140), PutWord(At(kCaptureH), 0xF0);
        PutLong(At(kBackBuffer), Addr(Next() % 2 ? kBackA : kBackB));
        At(kScreenBpp)[0] = static_cast<unsigned char>(Next() % 4 ? (Next() % 2 ? 2 : 4) : Next());
        const unsigned short cw = kControlWords[Next() % 3];
        const unsigned char fill = static_cast<unsigned char>(Next());
        Capture(g_start);

        std::memset(g_plain, fill, kPlainBytes);
        g_theirs.Clear();
        d3d_fuzz::g_log = &g_theirs;
        SetControlWord(cw);
        theirs();
        SetControlWord(saved_cw);
        Capture(g_theirs_state);
        const U plain_theirs = HashPlain();

        Restore(g_start);
        std::memset(g_plain, fill, kPlainBytes);
        g_ours.Clear();
        d3d_fuzz::g_log = &g_ours;
        SetControlWord(cw);
        D3d_AfterDraw();
        SetControlWord(saved_cw);
        d3d_fuzz::g_log = nullptr;

        // coverage, from the original's log
        bool locked_both = false, created = false;
        unsigned locks = 0;
        for (unsigned i = 0; i < g_theirs.n && i < d3d_fuzz::kMaxCalls; ++i) {
            if (g_theirs.calls[i].what == 0x219) ++locks;
            if (g_theirs.calls[i].what == 0x220) locked_both = true;
            if (g_theirs.calls[i].what == 0x13) created = true;
        }
        if (g_theirs.n == 0) ++cover.off;
        else if (first && !created) ++cover.first_failed;
        else if (!locked_both && locks) ++cover.lock_failed;
        if (locked_both) {
            // from the original's state after the call (a disturbance may have moved either since the copy)
            constexpr U kBlock = 0x80 + 8 + 0x400 + 4 + 0x20 + 4;   // where kCaptureReady's region starts
            if (first) ++(GetWord(g_theirs_state.bytes + kBlock + 4) < 0x140 ? cover.first_narrow : cover.first_wide);
            if (g_theirs_state.bytes[kBlock + 0x1C + 1 + 4] == 2) ++cover.narrow;
            else ++cover.wide;
        }

        char why[200] = "";
        bool same = Same(0, 0, why);
        if (same && HashPlain() != plain_theirs) {
            same = false;
            std::strcpy(why, "the captured pixels");
        }
        if (!same) {
            if (bad < 4) bof3::Log("shadow      d3d_rest MISMATCH: D3d_AfterDraw round %u (cw %04X): %s", r, cw, why);
            ++bad;
        }
    }
    return bad;
}

// --- Gfx_StoreImage --------------------------------------------------------------

constexpr U kStoreBytes = 0x10000;
unsigned char g_store[kStoreBytes], g_store_start[kStoreBytes], g_store_theirs[kStoreBytes];

struct StoreCover {
    unsigned empty, odd, rect_overwritten;
};

unsigned FuzzStore(StoreFn theirs, unsigned rounds, StoreCover& cover) {
    unsigned bad = 0;
    for (unsigned r = 0; r < rounds; ++r) {
        g_round = 0x800000u + r;
        const std::int32_t w = static_cast<std::int32_t>(Next() % 4 ? Next() % 0x41 : Next() % 0x200);
        std::int32_t h = static_cast<std::int32_t>(Next() % 8 == 0 ? Pick(std::initializer_list<U>{0, 0xFFFF, 0x8000, 0xFFFE}.begin(), 4)
                                                                   : Next() % 0x21);
        if (w * 2 * (h > 0 && h < 0x8000 ? h : 0) > static_cast<std::int32_t>(kStoreBytes - 0x10)) h = 1;
        const std::int32_t x = static_cast<std::int32_t>(Next() % 2 ? Next() % 0x400 : Pick(std::initializer_list<U>{0, 0x3FF, 0x340, 0x200}.begin(), 4));
        const std::int32_t y = static_cast<std::int32_t>(Next() % 2 ? Next() % 0x1E0 : Pick(std::initializer_list<U>{0, 0x100, 0x1DF}.begin(), 3));
        for (U i = 0; i < kStoreBytes; ++i) g_store_start[i] = static_cast<unsigned char>(Next());
        // the rectangle: on its own, or inside the rows the copy writes, with
        // the h it will be overwritten by planted in the shadow
        short own_rect[4];
        short* rect = own_rect;
        const U bytes = static_cast<U>(w) * 2;
        const std::int32_t hh = static_cast<std::int16_t>(h);
        if (Next() % 6 == 0 && bytes >= 8 && hh > 0 && hh < 0x20) {
            const U row = Next() % static_cast<U>(hh);
            const U q = (Next() % (bytes - 7)) & ~1u;
            rect = reinterpret_cast<short*>(g_store + row * bytes + q);
            const U source = kVram + ((static_cast<U>(y + static_cast<std::int32_t>(row)) << 10) + static_cast<U>(x)) * 2 + q + 6;
            if (source + 2 <= kVram + kVramBytes) {
                const U new_h = Pick(std::initializer_list<U>{0, 1, row + 1, row + 2, 0xFFFF, static_cast<U>(hh)}.begin(), 6);
                PutWord(At(source), new_h);
                ++cover.rect_overwritten;
            }
        }
        const short values[4] = {static_cast<short>(x), static_cast<short>(y), static_cast<short>(w), static_cast<short>(h)};
        unsigned char* rect_bytes = reinterpret_cast<unsigned char*>(rect);
        if (rect == own_rect) std::memcpy(own_rect, values, 8);
        else std::memcpy(g_store_start + (rect_bytes - g_store), values, 8);
        if (hh <= 0) ++cover.empty;
        if (bytes & 3) ++cover.odd;

        std::memcpy(g_store, g_store_start, kStoreBytes);
        if (rect == own_rect) std::memcpy(own_rect, values, 8);
        const int ret_theirs = theirs(rect, g_store);
        std::memcpy(g_store_theirs, g_store, kStoreBytes);

        std::memcpy(g_store, g_store_start, kStoreBytes);
        if (rect == own_rect) std::memcpy(own_rect, values, 8);
        const int ret_ours = Gfx_StoreImage(rect, g_store);

        const char* why = nullptr;
        if (ret_ours != ret_theirs) why = "the return value";
        else if (std::memcmp(g_store, g_store_theirs, kStoreBytes) != 0) why = "the copy";
        if (why) {
            if (bad < 4)
                bof3::Log("shadow      d3d_rest MISMATCH: Gfx_StoreImage(%d, %d, %d, %d) round %u: %s", (int)x, (int)y,
                          (int)w, (int)hh, r, why);
            ++bad;
        }
    }
    return bad;
}

}  // namespace

void SelfTest() {
    // The copies, before anything of ours is injected.
    const bof3::CloneCall texel_calls[] = {{0x48, P(StubClutPixels), bof3::addr::Gfx_ClutPixels}};
    const auto texel = Clone<TexelFn>("D3d_PageTexel4", bof3::addr::D3d_PageTexel4, 0x6A, texel_calls, 1);
    const bof3::CloneCall flatten_calls[] = {
        {0x2A, reinterpret_cast<const void*>(texel), bof3::addr::D3d_PageTexel4},
        {0x90, nullptr, kFtol},
        {0xC7, nullptr, kFtol},
        {0xFE, nullptr, kFtol},
    };
    const auto flatten = Clone<FlattenFn>("D3d_FlattenFT3", bof3::addr::D3d_FlattenFT3, 0x121, flatten_calls, 4);
    g_handlers[1].calls[0].target = reinterpret_cast<const void*>(flatten);
    DrawFn draws[kHandlerCount];
    for (unsigned i = 0; i < kHandlerCount; ++i) {
        const Handler& h = g_handlers[i];
        draws[i] = Clone<DrawFn>(h.name, h.original, h.size, h.calls, h.n_calls);
    }
    const auto alpha = Clone<AlphaFn>("D3d_SetAlphaModulate", bof3::addr::D3d_SetAlphaModulate, 0x53, nullptr, 0);
    const bof3::CloneCall after_calls[] = {
        {0x53, P(StubCreatePlain), bof3::addr::Dd_CreatePlainSurface},
        {0x83, P(StubFit), bof3::addr::D3d_FitTextureSize},
        {0xA9, P(StubCreateTexture), bof3::addr::Dd_CreateTextureSurface},
        {0x102, nullptr, kFtol},
        {0x119, nullptr, kFtol},
        {0x12C, nullptr, kFtol},
        {0x13A, P(StubInitDesc), bof3::addr::Dd_InitSurfaceDesc},
    };
    const auto after = Clone<AfterFn>("D3d_AfterDraw", bof3::addr::D3d_AfterDraw, 0x2BF, after_calls, 7);
    const auto store = Clone<StoreFn>("Gfx_StoreImage", bof3::addr::Gfx_StoreImage, 0x6C, nullptr, 0);

    g_palette_at = Addr(g_palette);
    FillSurfaceTraps(std::make_integer_sequence<int, 33>{});
    g_surface_vtbl[0x14 / 4] = reinterpret_cast<void*>(&FakeBlt);
    g_surface_vtbl[0x64 / 4] = reinterpret_cast<void*>(&FakeLock);
    g_surface_vtbl[0x80 / 4] = reinterpret_cast<void*>(&FakeUnlock);

    Capture(g_saved);
    std::memcpy(g_vram_saved, At(kVram), kVramBytes);
    const Callees saved_callees = g;
    g = {StubPrimColor, StubBind, StubRet, StubBlend, StubShade, StubClutPixels,
         StubInitDesc, StubCreatePlain, StubFit, StubCreateTexture};
    d3d_fuzz::Seed(0x59FA5059u);
    for (U i = 0; i < kVramBytes; ++i) At(kVram)[i] = static_cast<unsigned char>(Next());
    for (U i = 0; i < kBackBytes; ++i) g_back[i] = static_cast<unsigned char>(Next());

    unsigned bad = 0;
    constexpr unsigned kDrawRounds = 20000, kFlattenRounds = 40000, kAlphaRounds = 20000, kAfterRounds = 3000,
                       kStoreRounds = 20000;
    AfterCover after_cover = {};
    StoreCover store_cover = {};
    {
        d3d_fuzz::DeviceSwap swap;
        for (unsigned i = 0; i < kHandlerCount; ++i) bad += FuzzHandler(g_handlers[i], draws[i], kDrawRounds);
        bad += FuzzFlatten(flatten, texel, kFlattenRounds);
        bad += FuzzAlpha(alpha, kAlphaRounds);
        bad += FuzzAfterDraw(after, kAfterRounds, after_cover);
        bad += FuzzStore(store, kStoreRounds, store_cover);
    }

    g = saved_callees;
    Restore(g_saved);
    std::memcpy(At(kVram), g_vram_saved, kVramBytes);

    bof3::Log("shadow      d3d_rest self-test: five handlers %u rounds each under three control words (POLY_FT3 one "
              "texel %u), D3d_FlattenFT3 / D3d_PageTexel4 %u, D3d_SetAlphaModulate %u, D3d_AfterDraw %u (off %u, "
              "creation failed %u, first narrow %u / wide %u, a lock failed %u, copied 16-bit %u / 32-bit %u), "
              "Gfx_StoreImage %u (nothing %u, odd width %u, rectangle overwritten %u)",
              kDrawRounds, g_flattened, kFlattenRounds, kAlphaRounds, kAfterRounds, after_cover.off,
              after_cover.first_failed, after_cover.first_narrow, after_cover.first_wide, after_cover.lock_failed,
              after_cover.narrow, after_cover.wide, kStoreRounds, store_cover.empty, store_cover.odd,
              store_cover.rect_overwritten);
    if (bad) bof3::Fatal("the renderer's live remainder differs from the original in %u self-test rounds", bad);
}

}  // namespace d3d_rest
