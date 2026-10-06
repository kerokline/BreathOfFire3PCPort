// The renderer's live remainder, read to the last instruction with capstone
// against bof3/BOF3.exe (2026-10-05, docs/d3d-rest.md). All are the PC port's
// own: the PlayStation drew these primitives on its GPU, so there is no PSX twin
// to read them against. Faithful: no divergence.
//
//   Gfx_StoreImage         0x59E930..0x59E99B (0x6C)
//   D3d_SetAlphaModulate   0x59F520..0x59F572 (0x53)
//   D3d_AfterDraw          0x59F580..0x59F83E (0x2BF)
//   D3d_DrawPolyF3         0x59FA50..0x59FB9B (0x14C)  code 0x20
//   D3d_DrawPolyFT3        0x59FDB0..0x59FFD5 (0x226)  code 0x24
//   D3d_FlattenFT3         0x5A0910..0x5A0A30 (0x121)  under POLY_FT3
//   D3d_PageTexel4         0x5A0A40..0x5A0AA9 (0x6A)   under D3d_FlattenFT3
//   D3d_DrawPolyGT3        0x5A1050..0x5A1288 (0x239)  code 0x34
//   D3d_DrawLineG4         0x5A1EA0..0x5A20CE (0x22F)  code 0x5C
//   D3d_DrawTile1          0x5A2220..0x5A22F4 (0xD5)   code 0x68
//
// The arithmetic is x87 in the original and x87 here, instruction for
// instruction (the X87* helpers below, d3d_draw.cpp's idiom): each fmul and fdiv
// rounds to the control word's precision, a float that passes through fld /
// fstp comes out with a signalling NaN quieted, and the CRT's _ftol is a
// truncating fistp to 64 bits under a copy of the control word (area_w3g.cpp's
// Ftol). The fuzz checks three control words.
#include "game/d3d_rest.h"

#include <windows.h>

#include <cstdint>
#include <cstring>

#include "bof3/symbols.gen.h"
#include "game/d3d_rest_callees.h"
#include "hook/detour.h"
#include "hook/log.h"

namespace d3d_rest {

const Callees kOriginals = {
    D3d_PrimColor,
    D3d_BindTexture,
    reinterpret_cast<void (__cdecl*)(unsigned)>(static_cast<std::uintptr_t>(kRetOnly)),
    D3d_SetBlend,
    D3d_SetShadeMode,
    Gfx_ClutPixels,
    Dd_InitSurfaceDesc,
    Dd_CreatePlainSurface,
    D3d_FitTextureSize,
    Dd_CreateTextureSurface,
};
Callees g = kOriginals;

namespace {

unsigned char* At(U address) { return reinterpret_cast<unsigned char*>(static_cast<std::uintptr_t>(address)); }
U Addr(const void* p) { return static_cast<U>(reinterpret_cast<std::uintptr_t>(p)); }
U Word(const unsigned char* p) {
    std::uint16_t v;
    std::memcpy(&v, p, sizeof v);
    return v;
}
std::int32_t Short(const unsigned char* p) {
    std::int16_t v;
    std::memcpy(&v, p, sizeof v);
    return v;
}
U Long(const unsigned char* p) {
    U v;
    std::memcpy(&v, p, sizeof v);
    return v;
}
float Float(const unsigned char* p) {
    float v;
    std::memcpy(&v, p, sizeof v);
    return v;
}
float FloatAt(U address) { return Float(At(address)); }
void PutWord(unsigned char* p, U v) {
    const std::uint16_t w = static_cast<std::uint16_t>(v);
    std::memcpy(p, &w, sizeof w);
}
void PutLong(unsigned char* p, U v) { std::memcpy(p, &v, sizeof v); }
void PutFloat(unsigned char* p, float v) { std::memcpy(p, &v, sizeof v); }

// --- the x87 sequences, as the original executes them ------------------------

// fld a; fmul b; fstp
inline float X87Mul(float a, float b) {
    float r;
    __asm__ volatile("flds %1\n\tfmuls %2\n\tfstps %0" : "=m"(r) : "m"(a), "m"(b) : "st");
    return r;
}
// fld a; fdiv b; fstp
inline float X87Div(float a, float b) {
    float r;
    __asm__ volatile("flds %1\n\tfdivs %2\n\tfstps %0" : "=m"(r) : "m"(a), "m"(b) : "st");
    return r;
}
// fld a; fstp - a copy, except that a signalling NaN comes out quiet
inline float X87Pass(float a) {
    float r;
    __asm__ volatile("flds %1\n\tfstps %0" : "=m"(r) : "m"(a) : "st");
    return r;
}
// fild i; fmul k; fstp
inline float X87IntMul(std::int32_t i, float k) {
    float r;
    __asm__ volatile("fildl %1\n\tfmuls %2\n\tfstps %0" : "=m"(r) : "m"(i), "m"(k) : "st");
    return r;
}
// The CRT's _ftol 0x5B9550 on st0: round toward zero set in a copy of the
// control word for one fistp to 64 bits, the word put back; the low dword.
#define BOF3X_FTOL                       \
    "fnstcw %[saved]\n\t"                \
    "movw %[saved], %%ax\n\t"            \
    "orb $0x0C, %%ah\n\t"                \
    "movw %%ax, %[truncating]\n\t"       \
    "fldcw %[truncating]\n\t"            \
    "fistpll %[result]\n\t"              \
    "fldcw %[saved]\n\t"
// fld a; fmul b; _ftol
inline U X87MulFtol(float a, float b) {
    std::int64_t result;
    unsigned short saved, truncating;
    __asm__ volatile("flds %[a]\n\tfmuls %[b]\n\t" BOF3X_FTOL
                     : [result] "=m"(result), [saved] "=m"(saved), [truncating] "=m"(truncating)
                     : [a] "m"(a), [b] "m"(b)
                     : "eax", "st", "memory");
    return static_cast<U>(static_cast<std::uint64_t>(result));
}
// fild i; fmul b; _ftol
inline U X87IntMulFtol(std::int32_t i, float b) {
    std::int64_t result;
    unsigned short saved, truncating;
    __asm__ volatile("fildl %[i]\n\tfmuls %[b]\n\t" BOF3X_FTOL
                     : [result] "=m"(result), [saved] "=m"(saved), [truncating] "=m"(truncating)
                     : [i] "m"(i), [b] "m"(b)
                     : "eax", "st", "memory");
    return static_cast<U>(static_cast<std::uint64_t>(result));
}
// fild qword chan; fild dword c; fmul k; fmulp st(1); _ftol - D3d_FlattenFT3's red
inline U X87RedFtol(std::int64_t chan, std::int32_t c, float k) {
    std::int64_t result;
    unsigned short saved, truncating;
    __asm__ volatile("fildll %[chan]\n\tfildl %[c]\n\tfmuls %[k]\n\tfmulp %%st, %%st(1)\n\t" BOF3X_FTOL
                     : [result] "=m"(result), [saved] "=m"(saved), [truncating] "=m"(truncating)
                     : [chan] "m"(chan), [c] "m"(c), [k] "m"(k)
                     : "eax", "st", "st(1)", "memory");
    return static_cast<U>(static_cast<std::uint64_t>(result));
}
// fild dword c; fmul k; fimul dword chan; _ftol - its green and blue
inline U X87FimulFtol(std::int32_t c, float k, std::int32_t chan) {
    std::int64_t result;
    unsigned short saved, truncating;
    __asm__ volatile("fildl %[c]\n\tfmuls %[k]\n\tfimull %[chan]\n\t" BOF3X_FTOL
                     : [result] "=m"(result), [saved] "=m"(saved), [truncating] "=m"(truncating)
                     : [chan] "m"(chan), [c] "m"(c), [k] "m"(k)
                     : "eax", "st", "memory");
    return static_cast<U>(static_cast<std::uint64_t>(result));
}
#undef BOF3X_FTOL

// --- the COM objects ---------------------------------------------------------

// D3d_Device, read afresh before every call as the original does.
void** Device() { return *reinterpret_cast<void** volatile*>(At(kDevice)); }
void* Method(void** object, U offset) { return (*reinterpret_cast<void***>(object))[offset / 4]; }
using Com2 = long(__stdcall*)(void*, U);
using Com3 = long(__stdcall*)(void*, U, U);
using Com4 = long(__stdcall*)(void*, U, U, U);
using Com5 = long(__stdcall*)(void*, U, U, U, U);
using Com6 = long(__stdcall*)(void*, U, U, U, U, U);

long SetTexture(U stage, U texture) {
    void** device = Device();
    return reinterpret_cast<Com3>(Method(device, 0x98))(device, stage, texture);
}
// IDirect3DDevice3::SetTextureStageState(stage, type, value).
long SetStageState(U stage, U type, U value) {
    void** device = Device();
    return reinterpret_cast<Com4>(Method(device, 0xA0))(device, stage, type, value);
}
// DrawPrimitive(type, D3DFVF_TLVERTEX, D3d_Vertices, count, 0).
long DrawVertices(U type, U count) {
    void** device = Device();
    return reinterpret_cast<Com6>(Method(device, 0x70))(device, type, 0x1C4, kVertices, count, 0);
}
// A surface pointer read from its global at the moment of the call.
void** SurfaceAt(U address) { return *reinterpret_cast<void** volatile*>(At(address)); }
// IDirectDrawSurface4::Lock(rect, desc, flags, event), Unlock(rect), Blt(dst rect, src, src rect, flags, fx).
long Lock(void** surface, void* desc) {
    return reinterpret_cast<Com5>(Method(surface, 0x64))(surface, 0, Addr(desc), 1, 0);   // DDLOCK_WAIT
}
long Unlock(void** surface) { return reinterpret_cast<Com2>(Method(surface, 0x80))(surface, 0); }

unsigned char* Vertex(U i) { return At(kVertices + i * 0x20); }
U DrawMode() { return Long(At(kDrawTpage)) & 0xFFFF; }

// sx, sy = scale times the corner's float x, y; sz its z as it is (a mov);
// rhw = 0.1 / z.
void PutPosition(unsigned char* out, const unsigned char* xyz) {
    PutFloat(out + 0x00, X87Mul(FloatAt(kScaleX), Float(xyz)));
    PutFloat(out + 0x04, X87Mul(FloatAt(kScaleY), Float(xyz + 4)));
    PutLong(out + 0x08, Long(xyz + 8));
    PutFloat(out + 0x0C, X87Div(FloatAt(kRhwNumerator), Float(xyz + 8)));
}
// tu, tv = D3d_TexCoords[u], [v], through fld / fstp.
void PutTexel(unsigned char* out, const unsigned char* uv) {
    PutFloat(out + 0x18, X87Pass(FloatAt(kTexCoords + uv[0] * 4u)));
    PutFloat(out + 0x1C, X87Pass(FloatAt(kTexCoords + uv[1] * 4u)));
}

}  // namespace
}  // namespace d3d_rest

using namespace d3d_rest;

// --- the handlers ------------------------------------------------------------
// Each is reached from one site of Gfx_DrawOTag's Direct3D table (d3d-draw.md
// section 2) and returns what DrawPrimitive returned (the caller does not read
// it). Not kept, in all of them: the original lets the colour helper write the
// diffuse into the caller's pushed `prim` slot, which the caller pops unread.

// 0x59FA50, table entry 0 (code 0x20, called at 0x59F0AA): a flat untextured
// triangle. Three corners of 0xC bytes from +8 (float x, y, z); the colour of
// r, g, b +4..+6 and the code +7 in the blend mode Gfx_DrawTpage's low word,
// with a null specular - so the vertices' specular, tu and tv stay as the last
// draw left them; SetTexture(0, NULL); 0x437CC0(0) twice; the blend with
// Gfx_DrawTpage read again; flat; a triangle list of 3.
long D3d_DrawPolyF3(const unsigned char* prim) {
    unsigned long diffuse;
    g.prim_color(prim[4], prim[5], prim[6], prim[7], DrawMode(), &diffuse, nullptr);
    for (U i = 0; i < 3; ++i) {
        unsigned char* out = Vertex(i);
        PutPosition(out, prim + 8 + i * 0xC);
        PutLong(out + 0x10, static_cast<U>(diffuse));
    }
    SetTexture(0, 0);
    g.ret_only(0);
    g.ret_only(0);
    g.set_blend(prim[7], DrawMode());
    g.set_shade(1);   // flat
    return DrawVertices(4, 3);   // TRIANGLELIST
}

// 0x5A0A40, called only by D3d_FlattenFT3 (0x5A093A): the 4-bit texel at
// (u, v) of a texture page in Gfx_VramShadow, through a CLUT. The page from
// `page` as a tpage word: x = (page & 0xF) * 64 cells, y = 256 rows when bit
// 0x10; u = uv's low byte, v = uv >> 8 (arithmetic). The byte at
// Gfx_VramShadow + ((y + v) * 16 + page x) * 128 + ((uv >> 1) & 0x7F), and of it
// the HIGH nibble for an even u, the low for an odd (Tex_Convert4 reads the low
// nibble first: docs/d3d-rest.md section 4, latent). Then Gfx_ClutPixels(clut)
// - the converted palette - indexed by the nibble: a u16 when
// Gfx_PixelFormat's bytes per texel (+3, read after the call) is 2, else a
// dword.
U D3d_PageTexel4(U clut, U page, U uv) {
    U row = ((page & 0x10) << 4) + static_cast<U>(static_cast<std::int32_t>(uv) >> 8);
    const U offset = ((row << 4) + (page & 0xF)) << 7;
    const U column = static_cast<U>(static_cast<std::int32_t>(uv) >> 1) & 0x7F;
    const U texels = At(kVram + offset + column)[0];
    const U shift = (static_cast<U>(~uv) & 1) << 2;
    const U index = (texels >> shift) & 0xF;
    const unsigned char* palette = static_cast<const unsigned char*>(g.clut_pixels(clut));
    if (At(kPixelFormat + 3)[0] == 2) return Word(palette + index * 2);
    return Long(palette + index * 4);
}

// 0x5A0910, called only by D3d_DrawPolyFT3 (0x59FDCB) when the three corners'
// (u, v) words are equal: the triangle samples one texel, so it is drawn
// untextured in that texel's colour. The texel by D3d_PageTexel4(word +0xE,
// word +0x16, uv & 0xFFFF) - the PSX's POLY_FT3 offsets of the CLUT and the
// tpage, not this port's float layout, where +0xE is the high half of corner 0's
// y and +0x16 the CLUT (docs/d3d-rest.md section 4, latent). Its red, green
// and blue by Gfx_PixelFormat's masks and shifts, each to bits 16..23 then
// >> 16; the primitive's r, g, b each times its channel times 1/128 (x87,
// _ftol), clamped to 0xFF (unsigned), written back over the primitive - each
// read before the one before it is stored - and the code's bit 2 (textured)
// cleared. The primitive itself changes: drawn again without being rebuilt it
// is a POLY_F3 (code 0x20) of the modulated colour.
void D3d_FlattenFT3(unsigned char* prim, U uv) {
    const U pixel = D3d_PageTexel4(Word(prim + 0xE), Word(prim + 0x16), uv & 0xFFFF);
    const unsigned char* format = At(kPixelFormat);
    const U green = ((Long(format + 0x14) & pixel) << (Long(format + 0x8) & 31)) >> 16;
    const U blue = ((Long(format + 0x18) & pixel) << (Long(format + 0xC) & 31)) >> 16;
    const U red = ((Long(format + 0x10) & pixel) << (Long(format + 0x4) & 31)) >> 16;
    const float k = FloatAt(kModulate);
    U r = X87RedFtol(red, prim[4], k);
    if (r > 0xFF) r = 0xFF;
    const U gg = prim[5];
    prim[4] = static_cast<unsigned char>(r);
    U v = X87FimulFtol(static_cast<std::int32_t>(gg), k, static_cast<std::int32_t>(green));
    if (v > 0xFF) v = 0xFF;
    const U b = prim[6];
    prim[5] = static_cast<unsigned char>(v);
    v = X87FimulFtol(static_cast<std::int32_t>(b), k, static_cast<std::int32_t>(blue));
    if (v > 0xFF) v = 0xFF;
    prim[6] = static_cast<unsigned char>(v);
    prim[7] &= 0xFB;
}

// 0x59FDB0, table entry 1 (code 0x24, called at 0x59F0B8): a textured flat
// triangle. Three corners of 0x10 bytes from +8: float x, y, z, byte u, v; the
// CLUT word +0x16 and the tpage word +0x26 in the first two. If the three
// (u, v) words are equal, D3d_FlattenFT3 first (16-bit compares). Then one
// colour, with a specular, in the tpage's blend mode; the corners; the fourth
// vertex's z (0x7CA9C0) set to 0, as D3d_DrawPolyG3 does. Flattened: no
// texture coordinates and SetTexture(0, NULL); else tu, tv per corner and
// D3d_BindTexture(tpage, clut). 0x437CC0 with the tpage's bits 0x400 and 0x800,
// the blend, flat, a triangle list of 3 - the tpage word re-read after each
// call.
long D3d_DrawPolyFT3(unsigned char* prim) {
    bool flattened = false;
    const U uv = Word(prim + 0x14);
    if (uv == Word(prim + 0x24) && uv == Word(prim + 0x34)) {
        D3d_FlattenFT3(prim, uv);
        flattened = true;
    }
    unsigned long diffuse, specular;
    g.prim_color(prim[4], prim[5], prim[6], prim[7], Word(prim + 0x26), &diffuse, &specular);
    for (U i = 0; i < 3; ++i) {
        unsigned char* out = Vertex(i);
        PutPosition(out, prim + 8 + i * 0x10);
        PutLong(out + 0x10, static_cast<U>(diffuse));
        PutLong(out + 0x14, static_cast<U>(specular));
    }
    PutLong(Vertex(3) + 8, 0);
    if (flattened) {
        SetTexture(0, 0);
    } else {
        for (U i = 0; i < 3; ++i) PutTexel(Vertex(i), prim + 0x14 + i * 0x10);
        g.bind_texture(Word(prim + 0x26), Word(prim + 0x16));
    }
    g.ret_only(Word(prim + 0x26) & 0x400);
    g.ret_only(Word(prim + 0x26) & 0x800);
    g.set_blend(prim[7], Word(prim + 0x26));
    g.set_shade(1);   // flat
    return DrawVertices(4, 3);   // TRIANGLELIST
}

// 0x5A1050, table entry 5 (code 0x34, called at 0x59F0F0): a textured Gouraud
// triangle - D3d_DrawPolyGT4 with three corners. Corners of 0x14 bytes from +4:
// r, g, b, (code or pad), float x, y, z, byte u, v; the CLUT +0x16 and the tpage
// +0x2A. A colour per corner with a specular, each in the tpage's blend mode
// (re-read), all three before any vertex; per corner the position, the pair
// and the texel; D3d_BindTexture(tpage, clut); 0x437CC0(0) twice; the blend;
// Gouraud; a triangle list of 3.
long D3d_DrawPolyGT3(const unsigned char* prim) {
    unsigned long diffuse[3], specular[3];
    for (U i = 0; i < 3; ++i) {
        const unsigned char* rgb = prim + 4 + i * 0x14;
        g.prim_color(rgb[0], rgb[1], rgb[2], prim[7], Word(prim + 0x2A), &diffuse[i], &specular[i]);
    }
    for (U i = 0; i < 3; ++i) {
        unsigned char* out = Vertex(i);
        const unsigned char* in = prim + 8 + i * 0x14;
        PutPosition(out, in);
        PutLong(out + 0x10, static_cast<U>(diffuse[i]));
        PutLong(out + 0x14, static_cast<U>(specular[i]));
        PutTexel(out, in + 0xC);
    }
    g.bind_texture(Word(prim + 0x2A), Word(prim + 0x16));
    g.ret_only(0);
    g.ret_only(0);
    g.set_blend(prim[7], Word(prim + 0x2A));
    g.set_shade(2);   // Gouraud
    return DrawVertices(4, 3);   // TRIANGLELIST
}

// 0x5A1EA0, table entry 13 (code 0x5C, called at 0x59F160): a Gouraud
// three-segment polyline - D3d_DrawLineG3 (battle_draw.cpp) with four corners.
// Four colours (r, g, b at +4 + i * 0x10, the code +7 and Gfx_DrawTpage's low
// word read again for each, a null specular) before any vertex; four corners of
// 0x10 from +8 (float x, y, z); SetTexture(0, NULL); 0x437CC0(0), (1); the
// blend; Gouraud; a line strip of 4.
long D3d_DrawLineG4(const unsigned char* prim) {
    unsigned long diffuse[4];
    for (U i = 0; i < 4; ++i) {
        const unsigned char* rgb = prim + 4 + i * 0x10;
        g.prim_color(rgb[0], rgb[1], rgb[2], prim[7], DrawMode(), &diffuse[i], nullptr);
    }
    for (U i = 0; i < 4; ++i) {
        unsigned char* out = Vertex(i);
        PutPosition(out, prim + 8 + i * 0x10);
        PutLong(out + 0x10, static_cast<U>(diffuse[i]));
    }
    SetTexture(0, 0);
    g.ret_only(0);
    g.ret_only(1);
    g.set_blend(prim[7], DrawMode());
    g.set_shade(2);   // Gouraud
    return DrawVertices(3, 4);   // LINESTRIP
}

// 0x5A2220, table entry 16 (code 0x68, called at 0x59F17C): a one-pixel tile,
// drawn as ONE point - float x +8, y +0xC, z +0x10; the colour as the tile's
// (null specular, Gfx_DrawTpage); SetTexture(0, NULL); 0x437CC0(1) twice; the
// blend; flat; a point list of 1. At a scale of 2 that is one pixel of the four
// the PlayStation's pixel covers (docs/d3d-rest.md section 4).
//
// DIVERGENCE DIV-0077 (the owner, 2026-10-06): with g_tile1_quad on, the tile
// is the PlayStation pixel's whole footprint - a D3d_ScaleX by D3d_ScaleY quad
// from the scaled corner, as D3d_DrawTile draws a TILE of w = h = 1 (a
// triangle strip of four); the colour, blend and shade as the point's. The
// switch is set by D3dRest_Inject after the self-test, which compares the
// point; BOF3X_TILE1=0 leaves it off (the original's point).
unsigned char g_tile1_quad = 0;
long D3d_DrawTile1(const unsigned char* prim) {
    unsigned long diffuse;
    g.prim_color(prim[4], prim[5], prim[6], prim[7], DrawMode(), &diffuse, nullptr);
    if (g_tile1_quad != 0) {
        const float x = Float(prim + 8), y = Float(prim + 0xC), z = Float(prim + 0x10);
        const float left = X87Mul(FloatAt(kScaleX), x);
        const float top = X87Mul(FloatAt(kScaleY), y);
        const float right = X87Mul(FloatAt(kScaleX), x + 1.0f);
        const float bottom = X87Mul(FloatAt(kScaleY), y + 1.0f);
        const float rhw = X87Div(FloatAt(kRhwNumerator), z);
        const float xs[4] = {left, right, left, right}, ys[4] = {top, top, bottom, bottom};
        for (U i = 0; i < 4; ++i) {
            unsigned char* out = Vertex(i);
            PutFloat(out + 0x00, xs[i]);
            PutFloat(out + 0x04, ys[i]);
            PutLong(out + 0x08, Long(prim + 0x10));
            PutFloat(out + 0x0C, rhw);
            PutLong(out + 0x10, static_cast<U>(diffuse));
        }
        SetTexture(0, 0);
        g.ret_only(1);
        g.ret_only(1);
        g.set_blend(prim[7], DrawMode());
        g.set_shade(1);
        return DrawVertices(5, 4);   // TRIANGLESTRIP
    }
    unsigned char* out = Vertex(0);
    PutPosition(out, prim + 8);
    PutLong(out + 0x10, static_cast<U>(diffuse));
    SetTexture(0, 0);
    g.ret_only(1);
    g.ret_only(1);
    g.set_blend(prim[7], DrawMode());
    g.set_shade(1);
    return DrawVertices(1, 1);   // POINTLIST
}

// --- the walk's two other callees ----------------------------------------------

// 0x59F520, called by Gfx_DrawOTag for the codes 0xF4..0xF7 with byte +8 ^ 1
// (its only caller). Non-zero (a dword test): unless D3d_AlphaOpCache 0x66B720
// is already set, stage 0's ALPHAOP (4) to MODULATE (4) and the cache 1. Zero:
// unless the cache is already 0, ALPHAOP to SELECTARG2 (3) - the diffuse's
// alpha alone - and the cache 0. The cache is read once, before the call, and
// written after it. Nothing reads what it returns.
void D3d_SetAlphaModulate(unsigned on) {
    const U cache = Long(At(kAlphaOpCache));
    if (on != 0) {
        if (cache != 0) return;
        SetStageState(0, 4, 4);
        PutLong(At(kAlphaOpCache), 1);
    } else {
        if (cache == 0) return;
        SetStageState(0, 4, 3);
        PutLong(At(kAlphaOpCache), 0);
    }
}

// 0x59F580, a tail jump of Gfx_DrawOTag when D3d_AfterDrawRequest is set after
// the walk (nothing in .text sets it - docs/d3d-rest.md section 5). A capture of
// the frame into a texture: nothing at all under Gfx_RenderFlags bit 0.
//
// On first use (the capture surface 0x7CADF8 null): the scale 1.0, the used
// size 0x140 x 0xF0; a plain surface of 0x140 x 0x140 in the screen's format
// (2); D3d_FitTextureSize(0x140, 0xF0) into the dwords 0x7CADF0 / 0x7CADF2 -
// which overlap - and a texture surface of the low words; if the texture is
// narrower than 0x140, the used width becomes its width, the scale width /
// 320 (fild, fmul, fstp) and the used height 0xF0 times the scale (_ftol).
// Either creation failing returns at once.
//
// Then: the steps ScaleX and ScaleY times 65536 (_ftol), 16.16; one surface
// description, locked through the capture surface (its pixels kept) and then
// through DDraw_BackBuffer (its pixels and pitch read from it); either lock
// failing returns - the second leaving the capture surface locked. 240 rows of
// 320 pixels, nearest, from the back buffer at (x * step >> 16, y * step >> 16)
// into the capture surface packed (its pitch never read): 16-bit when the
// screen's bytes per pixel (0x7DEDE3, read after the locks) is 2, else 32. The
// back buffer unlocked, then the capture surface; Blt of the capture surface's
// (0, 0, 320, 240) onto the texture's (0, 0, used w, used h), DDBLT_WAIT; and
// the ready word 0x7CADE8 set to 1, whatever Blt answered. Nothing in .text but
// Display_Setup and the teardown reads the ready word or the texture.
void D3d_AfterDraw() {
    if (At(kRenderFlags)[0] & 1) return;
    if (Long(At(kCaptureSurface)) == 0) {
        PutLong(At(kCaptureScale), 0x3F800000);   // 1.0
        PutWord(At(kCaptureW), 0x140);
        PutWord(At(kCaptureH), 0xF0);
        if (!g.create_plain(0x140, 0x140, reinterpret_cast<void**>(At(kCaptureSurface)), 2)) return;
        g.fit_size(Long(At(kCaptureW)) & 0xFFFF, Word(At(kCaptureH)), reinterpret_cast<unsigned*>(At(kCaptureTexW)),
                   reinterpret_cast<unsigned*>(At(kCaptureTexH)));
        if (!g.create_texture(Long(At(kCaptureTexW)) & 0xFFFF, Word(At(kCaptureTexH)),
                              reinterpret_cast<void**>(At(kCaptureTexSurface)),
                              reinterpret_cast<void**>(At(kCaptureTexture)), 2))
            return;
        const U width = Word(At(kCaptureTexW));
        if (width < 0x140) {
            const U height = Word(At(kCaptureH));
            PutWord(At(kCaptureW), width);
            PutFloat(At(kCaptureScale), X87IntMul(static_cast<std::int32_t>(width), FloatAt(kPerColumn)));
            PutWord(At(kCaptureH), X87IntMulFtol(static_cast<std::int32_t>(height), FloatAt(kCaptureScale)));
        }
    }
    const U step_x = X87MulFtol(FloatAt(kScaleX), FloatAt(kFixedOne));
    const U step_y = X87MulFtol(FloatAt(kScaleY), FloatAt(kFixedOne));
    alignas(4) unsigned char desc[0x7C];
    g.init_desc(desc);
    if (Lock(SurfaceAt(kCaptureSurface), desc) != 0) return;
    U out = Long(desc + 0x24);
    if (Lock(SurfaceAt(kBackBuffer), desc) != 0) return;
    const bool narrow = At(kScreenBpp)[0] == 2;
    const U from = Long(desc + 0x24);
    U y = 0;
    for (U row = 0; row < 0xF0; ++row) {
        // sar, then imul: the low 32 bits, which an unsigned multiply gives too
        const U line = static_cast<U>(static_cast<std::int32_t>(y) >> 16) * Long(desc + 0x10);
        U x = 0;
        for (U column = 0; column < 0x140; ++column) {
            if (narrow) {
                const U at = ((x >> 15) & 0x1FFFE) + line;
                std::memcpy(At(out), At(from + at), 2);
                out += 2;
            } else {
                const U at = ((x >> 14) & 0x3FFFC) + line;
                std::memcpy(At(out), At(from + at), 4);
                out += 4;
            }
            x += step_x;
        }
        y += step_y;
    }
    Unlock(SurfaceAt(kBackBuffer));
    Unlock(SurfaceAt(kCaptureSurface));
    const U dst_rect[4] = {0, 0, Long(At(kCaptureW)) & 0xFFFF, Word(At(kCaptureH))};
    const U src_rect[4] = {0, 0, 0x140, 0xF0};
    void** source = SurfaceAt(kCaptureSurface);
    void** texture = SurfaceAt(kCaptureTexSurface);
    reinterpret_cast<Com6>(Method(texture, 0x14))(texture, Addr(dst_rect), Addr(source), Addr(src_rect), 0x1000000, 0);
    PutWord(At(kCaptureReady), 1);
}

// --- the VRAM shadow's read-back -------------------------------------------------

// 0x59E930 (no PSX pair read; the PC counterpart of libgpu's StoreImage, as
// Gfx_LoadImage 0x59EA70 is of LoadImage): the rectangle (s16 x, y, w, h) of
// Gfx_VramShadow copied row by row to `to`, packed (w * 2 bytes a row). x, y
// and w are read once; h is read again after every row (through the rectangle's
// pointer: a rectangle the copy overwrites changes the count), and nothing is
// copied when it is not above 0 (16-bit, signed). No bounds are checked: a
// rectangle outside the 1024 x 512 shadow reads past it, and a negative w is a
// copy of about 4 GB - as the original. Each row a forward copy of dwords then
// the 0..3 bytes left (rep movsd, rep movsb). Returns w * 2, which none of its
// four callers (the effect states 0x475EC0, 0x480730, 0x490BB0, 0x491410, all
// ours) reads.
int Gfx_StoreImage(const short* rect, void* to) {
    const unsigned char* r = reinterpret_cast<const unsigned char*>(rect);
    const U x = static_cast<U>(Short(r)), y = static_cast<U>(Short(r + 2));
    U from = kVram + ((y << 10) + x) * 2;
    const U bytes = static_cast<U>(Short(r + 4)) << 1;
    if (Short(r + 6) <= 0) return static_cast<int>(bytes);
    U dest = Addr(to);
    std::int32_t i = 0;
    do {
        U s = from, d = dest;
        for (U n = bytes >> 2; n != 0; --n, s += 4, d += 4) {
            U v;
            std::memcpy(&v, At(s), 4);
            std::memcpy(At(d), &v, 4);
        }
        for (U n = bytes & 3; n != 0; --n, ++s, ++d) At(d)[0] = At(s)[0];
        from += 0x800;
        dest += bytes;
        ++i;
    } while (i < Short(r + 6));
    return static_cast<int>(bytes);
}

void D3dRest_Inject() {
    if (bof3::WantsShadow("d3d_rest")) d3d_rest::SelfTest();
    // DIVERGENCE DIV-0077: after the self-test, which compares Capcom's point.
    {
        char text[16];
        const DWORD n = GetEnvironmentVariableA("BOF3X_TILE1", text, sizeof text);
        const bool off = n == 1 && text[0] == '0';
        if (n > 1 || (n == 1 && text[0] != '0' && text[0] != '1')) bof3::Fatal("BOF3X_TILE1 must be 0 or 1");
        if (!off) {
            static const std::uint8_t was = 0, is = 1;
            bof3::PatchBytes("Tile1Quad", static_cast<std::uint32_t>(reinterpret_cast<std::uintptr_t>(&g_tile1_quad)),
                             &was, &is, 1);
            bof3::Log("DIV-0077    TILE_1 drawn as a scale-sized quad, the PlayStation pixel's footprint (BOF3X_TILE1=0 for the point)");
        }
    }
    BOF3_INJECT(Gfx_StoreImage);
    BOF3_INJECT(D3d_SetAlphaModulate);
    BOF3_INJECT(D3d_AfterDraw);
    BOF3_INJECT(D3d_DrawPolyF3);
    BOF3_INJECT(D3d_DrawPolyFT3);
    BOF3_INJECT(D3d_FlattenFT3);
    BOF3_INJECT(D3d_PageTexel4);
    BOF3_INJECT(D3d_DrawPolyGT3);
    BOF3_INJECT(D3d_DrawLineG4);
    BOF3_INJECT(D3d_DrawTile1);
}
