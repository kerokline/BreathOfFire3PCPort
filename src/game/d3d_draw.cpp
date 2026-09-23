// The Direct3D draw handlers, their state helpers and the two texture lookups
// under them, read to the last instruction with capstone against bof3/BOF3.exe
// (docs/d3d-draw.md). All are the PC port's own: the PlayStation drew these
// primitives on its GPU, so there is no PSX twin to read them against. Faithful:
// no divergence.
//
//   D3d_PrimColor          0x59FBA0..0x59FC83 (0xE4), jump table 0x59FC84
//   D3d_SetBlend           0x59FCA0..0x59FD68 (0xC9), jump table 0x59FD6C
//   D3d_SetShadeMode       0x59FD80..0x59FDA3 (0x24)
//   D3d_BindTexture        0x59FFE0..0x5A0076 (0x97)
//   D3d_DrawPolyFT4        0x5A0C40..0x5A0E7C (0x23D)  code 0x2C
//   D3d_DrawPolyGT4        0x5A14C0..0x5A1793 (0x2D4)  code 0x3C
//   D3d_DrawLineF2         0x5A17A0..0x5A18AE (0x10F)  code 0x40
//   D3d_DrawLineF4         0x5A1D10..0x5A1E94 (0x185)  code 0x4C
//   D3d_DrawTile           0x5A20D0..0x5A2219 (0x14A)  code 0x60
//   D3d_DrawCellSprite     0x5A2EB0..0x5A3159 (0x2AA)  code 0x84
//   D3d_CellTexture        0x5A3160..0x5A32A3 (0x144)
//
// The arithmetic is x87 in the original and x87 here, instruction for
// instruction (the X87* helpers below): each fmul, fadd and fdiv rounds to the
// control word's precision, and a float that passes through fld / fstp comes
// out with a signalling NaN quieted, so only the same sequence gives the same
// bits under every control word. The fuzz checks three (0x027F, the game's
// measured one, 0x007F and 0x037F).
#include "game/d3d_draw.h"

#include <cstdint>
#include <cstring>

#include "bof3/symbols.gen.h"
#include "game/d3d_draw_callees.h"
#include "hook/detour.h"
#include "hook/log.h"

namespace d3d_draw {

const Callees kOriginals = {
    D3d_PrimColor,
    D3d_BindTexture,
    reinterpret_cast<void (__cdecl*)(unsigned)>(static_cast<std::uintptr_t>(kRetOnly)),
    D3d_SetBlend,
    D3d_SetShadeMode,
    D3d_CellTexture,
    Gfx_TexCacheFind,
    D3d_BuildPageTexture,
    D3d_RefreshPageTexture,
    D3d_BuildCellTexture,
    D3d_RefreshCellTexture,
};
Callees g = kOriginals;

namespace {

unsigned char* At(U address) { return reinterpret_cast<unsigned char*>(static_cast<std::uintptr_t>(address)); }
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
// fld a; fadd b; fmul c; fstp - (a + b) * c
inline float X87AddMul(float a, float b, float c) {
    float r;
    __asm__ volatile("flds %1\n\tfadds %2\n\tfmuls %3\n\tfstps %0" : "=m"(r) : "m"(a), "m"(b), "m"(c) : "st");
    return r;
}
// fild i; fmul s; fadd b (or fsubr b: b - that); fmul c; fstp
inline float X87IntMulAddMul(std::int32_t i, float s, float b, float c, bool subtract) {
    float r;
    if (subtract)
        __asm__ volatile("fildl %1\n\tfmuls %2\n\tfsubrs %3\n\tfmuls %4\n\tfstps %0"
                         : "=m"(r)
                         : "m"(i), "m"(s), "m"(b), "m"(c)
                         : "st");
    else
        __asm__ volatile("fildl %1\n\tfmuls %2\n\tfadds %3\n\tfmuls %4\n\tfstps %0"
                         : "=m"(r)
                         : "m"(i), "m"(s), "m"(b), "m"(c)
                         : "st");
    return r;
}
// k / n, n an integer (exactly representable: the original's fild, or fild and
// fstp to a float, of a u16)
inline float X87DivInt(float k, std::int32_t n) {
    float r;
    __asm__ volatile("flds %1\n\tfidivl %2\n\tfstps %0" : "=m"(r) : "m"(k), "m"(n) : "st");
    return r;
}
// fild e; fsub k; then / n
inline float X87IntSubDivInt(std::int32_t e, float k, std::int32_t n) {
    float r;
    __asm__ volatile("fildl %1\n\tfsubs %2\n\tfidivl %3\n\tfstps %0" : "=m"(r) : "m"(e), "m"(k), "m"(n) : "st");
    return r;
}

// --- the device --------------------------------------------------------------

// D3d_Device, read afresh before every COM call as the original does.
void** Device() { return *reinterpret_cast<void** volatile*>(At(0x7CC350)); }
void* Method(void** device, U offset) { return (*reinterpret_cast<void***>(device))[offset / 4]; }
using Com1 = long(__stdcall*)(void*);
using Com3 = long(__stdcall*)(void*, U, U);
using Com6 = long(__stdcall*)(void*, U, U, U, U, U);

long SetRenderState(U state, U value) {
    void** device = Device();
    return reinterpret_cast<Com3>(Method(device, 0x58))(device, state, value);
}
long SetTexture(U stage, U texture) {
    void** device = Device();
    return reinterpret_cast<Com3>(Method(device, 0x98))(device, stage, texture);
}
long Scene(U offset) {   // 0x24 BeginScene, 0x28 EndScene
    void** device = Device();
    return reinterpret_cast<Com1>(Method(device, offset))(device);
}
// DrawPrimitive(type, D3DFVF_TLVERTEX, D3d_Vertices, count, 0).
long DrawVertices(U type, U count) {
    void** device = Device();
    return reinterpret_cast<Com6>(Method(device, 0x70))(device, type, 0x1C4, kVertices, count, 0);
}

unsigned char* Vertex(U i) { return At(kVertices + i * 0x20); }

// sx, sy = scale times the primitive's float x, y; sz the primitive's z as it
// is (a mov); rhw = 0.1 / z.
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
}  // namespace d3d_draw

using namespace d3d_draw;

// --- the helpers -----------------------------------------------------------

// 0x59FBA0. A primitive's PSX colour as a Direct3D diffuse / specular pair.
// Only the low byte of `code` is tested; r, g, b are whole dwords (every caller
// passes a byte zero-extended). Alpha 0xFF, unless code bit 1 (semi-transparent):
// then by the tpage's blend field, (mode >> 5) & 3, through the table 0x59FC84 -
// 0x80, 0xFF, 0xFF, 0x40. Colour: bit 2 clear (untextured) - r, g, b as given,
// OR-ed in unmasked; bits 2 and 0 (texture, raw) - 0xFF each; bit 2 alone
// (texture modulated) - each doubled (32-bit, so it wraps), and a doubled value
// above 0xFF becomes 0xFF with its low byte going to the specular. The specular
// is written only through a non-null pointer, after the diffuse.
void D3d_PrimColor(unsigned r, unsigned g, unsigned b, unsigned code, unsigned mode, unsigned long* diffuse,
                   unsigned long* specular) {
    const unsigned char c = static_cast<unsigned char>(code);
    U alpha = 0xFF;
    if (c & 2) {
        static constexpr U kAlpha[4] = {0x80, 0xFF, 0xFF, 0x40};
        alpha = kAlpha[(mode >> 5) & 3];
    }
    U red = r, green = g, blue = b;
    U excess_r = 0, excess_g = 0, excess_b = 0;
    if (c & 4) {
        if (c & 1) {
            red = green = blue = 0xFF;
        } else {
            red = r << 1;
            if (red > 0xFF) {
                excess_r = red & 0xFF;
                red = 0xFF;
            }
            green = g << 1;
            if (green > 0xFF) {
                excess_g = green & 0xFF;
                green = 0xFF;
            }
            blue = b << 1;
            if (blue > 0xFF) {
                excess_b = blue & 0xFF;
                blue = 0xFF;
            }
        }
    }
    *diffuse = ((((alpha << 8) | red) << 8 | green) << 8) | blue;
    if (specular) *specular = ((excess_r << 8) | excess_g) << 8 | excess_b;
}

// 0x59FCA0. The PSX semi-transparency as three render states: ALPHABLENDENABLE
// (0x1B) on, SRCBLEND (0x13), DESTBLEND (0x14). Code bit 1 (the low byte only)
// clear: 5, 6 (SRCALPHA, INVSRCALPHA - the alpha is 0xFF, so opaque). Set: by
// (mode >> 5) & 3 through 0x59FD6C - 5, 6 / 2, 2 / 1, 4 / 5, 2.
void D3d_SetBlend(unsigned code, unsigned mode) {
    static constexpr U kBlend[4][2] = {{5, 6}, {2, 2}, {1, 4}, {5, 2}};
    const U* pair = (code & 2) ? kBlend[(mode >> 5) & 3] : kBlend[0];
    SetRenderState(0x1B, 1);
    SetRenderState(0x13, pair[0]);
    SetRenderState(0x14, pair[1]);
}

// 0x59FD80. SHADEMODE (9), only when it differs from the cache; the cache is
// written after the call.
void D3d_SetShadeMode(unsigned mode) {
    if (*reinterpret_cast<volatile U*>(At(kShadeCache)) == mode) return;
    SetRenderState(9, mode);
    *reinterpret_cast<volatile U*>(At(kShadeCache)) = mode;
}

// 0x59FFE0. The texture of a PSX texture page and CLUT, found in or built into
// Gfx_TexCache, set on stage 0. Page = tpage & 0x1F, colour mode =
// (tpage >> 7) & 3. A miss (slot 0x20) builds - outside the scene - and sets
// what the builder returns; a hit sets the entry's texture, and an entry in
// state 2 (its CLUT changed) is refreshed outside the scene first. Kept: the
// texture handed to SetTexture on that path is the one read BEFORE the refresh.
long D3d_BindTexture(unsigned tpage, unsigned clut) {
    const int page = static_cast<int>(tpage & 0x1F);
    const int mode = static_cast<int>((tpage >> 7) & 3);
    const int slot = g.tex_find(page, static_cast<int>(clut), mode);
    U texture;
    if (slot == 0x20) {
        Scene(0x28);   // EndScene
        texture = static_cast<U>(g.tex_build(page, static_cast<int>(clut), mode));
        Scene(0x24);   // BeginScene
    } else {
        const unsigned char* e = At(kPageCache + static_cast<U>(page * 32 + slot) * 0x18);
        const unsigned char state = e[0];
        texture = Long(e + 0x14);
        if (state == 2) {
            Scene(0x28);
            g.tex_refresh(page, slot);
            Scene(0x24);
        }
    }
    return SetTexture(0, texture);
}

// --- the handlers ------------------------------------------------------------
// Each is reached from one site of the draw 0x59EE50's second jump table and
// returns what DrawPrimitive returned (the caller does not read it). Not kept,
// in all of them: the original lets the colour helper write the diffuse into
// the caller's pushed `prim` slot, which the caller pops unread.

// 0x5A0C40, code 0x2C (Gpu_SetPolyFT4): a textured flat quad. Four corners of
// 0x10 bytes from +8: float x, y, z, u, v, and the CLUT (+0x16) / tpage (+0x26)
// words in the first two. One colour for all four, in the tpage's blend mode.
long D3d_DrawPolyFT4(const unsigned char* prim) {
    unsigned long diffuse, specular;
    g.prim_color(prim[4], prim[5], prim[6], prim[7], Word(prim + 0x26), &diffuse, &specular);
    for (U i = 0; i < 4; ++i) {
        unsigned char* out = Vertex(i);
        PutPosition(out, prim + 8 + i * 0x10);
        PutLong(out + 0x10, static_cast<U>(diffuse));
        PutLong(out + 0x14, static_cast<U>(specular));
    }
    for (U i = 0; i < 4; ++i) PutTexel(Vertex(i), prim + 0x14 + i * 0x10);
    g.bind_texture(Word(prim + 0x26), Word(prim + 0x16));
    g.ret_only(Word(prim + 0x26) & 0x400);
    g.ret_only(Word(prim + 0x26) & 0x800);
    g.set_blend(prim[7], Word(prim + 0x26));
    g.set_shade(1);   // flat
    return DrawVertices(5, 4);   // TRIANGLESTRIP
}

// 0x5A14C0, code 0x3C (Gpu_SetPolyGT4): a textured Gouraud quad. Four corners
// of 0x14 bytes from +4: r, g, b, (code or pad), float x, y, z, u, v, and the
// CLUT (+0x16) / tpage (+0x2A) words. A colour per corner, each in the tpage's
// blend mode, all four computed before any vertex is written.
long D3d_DrawPolyGT4(const unsigned char* prim) {
    unsigned long diffuse[4], specular[4];
    for (U i = 0; i < 4; ++i) {
        const unsigned char* rgb = prim + 4 + i * 0x14;
        g.prim_color(rgb[0], rgb[1], rgb[2], prim[7], Word(prim + 0x2A), &diffuse[i], &specular[i]);
    }
    for (U i = 0; i < 4; ++i) {
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
    return DrawVertices(5, 4);
}

namespace {
// The line handlers and the tile: no texture (SetTexture(0, NULL)), the blend
// mode from Gfx_DrawTpage - read before the colour and read again for the
// blend - and no specular: the colour helper is handed a null pointer, so the
// vertices' specular, tu and tv are left as the last draw left them.
long DrawLines(const unsigned char* prim, U corners) {
    const U mode = Long(At(kDrawTpage)) & 0xFFFF;
    unsigned long diffuse;
    g.prim_color(prim[4], prim[5], prim[6], prim[7], mode, &diffuse, nullptr);
    for (U i = 0; i < corners; ++i) {
        unsigned char* out = Vertex(i);
        PutPosition(out, prim + 8 + i * 0xC);
        PutLong(out + 0x10, static_cast<U>(diffuse));
    }
    SetTexture(0, 0);
    g.ret_only(0);
    g.ret_only(1);
    g.set_blend(prim[7], Long(At(kDrawTpage)) & 0xFFFF);
    g.set_shade(1);
    return DrawVertices(3, corners);   // LINESTRIP
}
}  // namespace

// 0x5A17A0, code 0x40 (Gpu_SetLineF2): one flat line, two corners of 0xC bytes
// from +8 (float x, y, z).
long D3d_DrawLineF2(const unsigned char* prim) { return DrawLines(prim, 2); }

// 0x5A1D10, code 0x4C (Gpu_SetLineF4): a flat three-segment polyline, four
// corners of 0xC bytes from +8.
long D3d_DrawLineF4(const unsigned char* prim) { return DrawLines(prim, 4); }

// 0x5A20D0, code 0x60 (Gpu_SetTile): a flat untextured rectangle - float x
// +8, y +0xC, z +0x10, w +0x14, h +0x18. The far edges are (x + w) and (y + h)
// times the scale, summed and multiplied on the x87 as the original does; z goes
// through fld / fst to all four corners (a signalling NaN comes out quiet, unlike
// the other handlers' mov).
long D3d_DrawTile(const unsigned char* prim) {
    const U mode = Long(At(kDrawTpage)) & 0xFFFF;
    unsigned long diffuse;
    g.prim_color(prim[4], prim[5], prim[6], prim[7], mode, &diffuse, nullptr);
    const float x = Float(prim + 8), y = Float(prim + 0xC), z = Float(prim + 0x10);
    const float left = X87Mul(FloatAt(kScaleX), x);
    const float top = X87Mul(FloatAt(kScaleY), y);
    const float right = X87AddMul(Float(prim + 0x14), x, FloatAt(kScaleX));
    const float bottom = X87AddMul(Float(prim + 0x18), y, FloatAt(kScaleY));
    const float sz = X87Pass(z);
    const float rhw = X87Div(FloatAt(kRhwNumerator), z);
    const float xs[4] = {left, right, left, right}, ys[4] = {top, top, bottom, bottom};
    for (U i = 0; i < 4; ++i) {
        unsigned char* out = Vertex(i);
        PutFloat(out + 0x00, xs[i]);
        PutFloat(out + 0x04, ys[i]);
        PutFloat(out + 0x08, sz);
        PutFloat(out + 0x0C, rhw);
        PutLong(out + 0x10, static_cast<U>(diffuse));
    }
    SetTexture(0, 0);
    g.ret_only(1);
    g.ret_only(1);
    g.set_blend(prim[7], Long(At(kDrawTpage)) & 0xFFFF);
    g.set_shade(1);
    return DrawVertices(5, 4);
}

// 0x5A2EB0, code 0x84 (Gpu_SetCode84 - no PSX GPU code; the port's): a sprite
// made of SpriteCell records, drawn from one texture D3d_CellTexture composes
// and caches. Float x +8, y +0xC, scale x +0x10, scale y +0x14; u16 first cell
// +0x18, cell count +0x1A, CLUT +0x1C, flags +0x1E (bit 0x400 flips x, and it
// is the colour's and the blend's tpage). The cache entry gives the sprite's
// extent in cell units (s16 left +8, right +0xA, top +0xC, bottom +0xE) and
// its texture's used size (u16 +0, +2) and full size (u16 +4, +6); the texture
// coordinates run from half a texel in to half a texel short of the used size.
// sz 0.99 and rhw 0.1 as immediates. The entry index is not checked: an index
// out of the table reads past it, as the original does.
long D3d_DrawCellSprite(const unsigned char* prim) {
    unsigned long diffuse, specular;
    g.prim_color(prim[4], prim[5], prim[6], prim[7], Word(prim + 0x1E), &diffuse, &specular);
    const int index = g.cell_texture(Word(prim + 0x18), Word(prim + 0x1A), Word(prim + 0x1C), Word(prim + 0x1E));
    const unsigned char* e = At(kCellCache + static_cast<U>(index) * kCellEntry);
    const bool flip = (prim[0x1F] & 4) != 0;
    const float x = Float(prim + 8), y = Float(prim + 0xC), sx = Float(prim + 0x10), sy = Float(prim + 0x14);
    const float left = X87IntMulAddMul(Short(e + 8), sx, x, FloatAt(kScaleX), flip);
    const float right = X87IntMulAddMul(Short(e + 0xA), sx, x, FloatAt(kScaleX), flip);
    const float top = X87IntMulAddMul(Short(e + 0xC), sy, y, FloatAt(kScaleY), false);
    const float bottom = X87IntMulAddMul(Short(e + 0xE), sy, y, FloatAt(kScaleY), false);
    const float half = FloatAt(kHalf);
    const auto used_w = static_cast<std::int32_t>(Word(e + 0)), used_h = static_cast<std::int32_t>(Word(e + 2));
    const auto full_w = static_cast<std::int32_t>(Word(e + 4)), full_h = static_cast<std::int32_t>(Word(e + 6));
    const float u0 = X87DivInt(half, full_w), v0 = X87DivInt(half, full_h);
    const float u1 = X87IntSubDivInt(used_w, half, full_w), v1 = X87IntSubDivInt(used_h, half, full_h);
    const float xs[4] = {left, right, left, right}, ys[4] = {top, top, bottom, bottom};
    const float us[4] = {u0, u1, u0, u1}, vs[4] = {v0, v0, v1, v1};
    for (U i = 0; i < 4; ++i) {
        unsigned char* out = Vertex(i);
        PutFloat(out + 0x00, xs[i]);
        PutFloat(out + 0x04, ys[i]);
        PutLong(out + 0x08, 0x3F7D70A4);   // 0.99
        PutLong(out + 0x0C, 0x3DCCCCCD);   // 0.1
        PutLong(out + 0x10, static_cast<U>(diffuse));
        PutLong(out + 0x14, static_cast<U>(specular));
        PutFloat(out + 0x18, us[i]);
        PutFloat(out + 0x1C, vs[i]);
    }
    g.ret_only(1);
    g.set_blend(prim[7], Word(prim + 0x1E));
    g.set_shade(1);
    return DrawVertices(5, 4);
}

// 0x5A3160. Finds - or builds - the texture composed from `count` SpriteCell
// records from `first` in one CLUT, sets it on stage 0 and returns its index in
// D3d_CellTexCache: 128 entries of 0x28 - u16 used w, h +0, +2; u16 texture w,
// h +4, +6; s16 extent +8..+0xE; u16 CLUT +0x10; u16 count +0x12; u16 used this
// frame +0x14; u16 use counter +0x16; the cells' checksum +0x18; the CLUT row's
// generation +0x1C; surface +0x20; texture +0x24.
//
// One pass: an empty entry (its first dword 0) is built at once. A hit is the
// count and CLUT words equal to the arguments (32-bit compares, words
// zero-extended) and the checksum - the 32-bit sum of the records' 2 * count
// dwords - equal; a hit whose CLUT row has changed is refreshed unless it was
// used this frame already. On the way, the victim: of the entries not used this
// frame, the one with the smallest use counter below 0xFFFF, the first on a tie.
// No hit and no empty entry: the victim is built.
//
// Kept, and D27 in docs/known-defects.md: the victim's index lives in the
// stack slot of the `count` argument, so when no entry qualifies - all 128 used
// this frame - the "victim" is `count` itself, and the build, the counter and
// the in-use word land at D3d_CellTexCache + count * 0x28, past the table.
int D3d_CellTexture(unsigned first, unsigned count, unsigned clut, unsigned flags) {
    U best = 0xFFFF;
    U victim = count;
    U i;
    for (i = 0; i < kCellEntries; ++i) {
        const unsigned char* e = At(kCellCache + i * kCellEntry);
        if (Long(e) == 0) {
            g.cell_build(static_cast<int>(i), first, count, clut, flags & 0x800);
            goto used;
        }
        if (Word(e + 0x12) == count && Word(e + 0x10) == clut) {
            U sum = 0;
            const U bytes = count * 8;
            for (U k = 0; k < bytes; k += 4) sum += Long(At(kCells + first * 8 + k));
            if (Long(e + 0x18) == sum) {
                if (Long(e + 0x1C) != Long(At(kClutRows + (clut >> 6) * 8)) && Word(e + 0x14) == 0)
                    g.cell_refresh(static_cast<int>(i), first, count, clut);
                goto used;
            }
        }
        if (static_cast<std::int32_t>(best) > static_cast<std::int32_t>(Word(e + 0x16)) && Word(e + 0x14) == 0) {
            best = Word(e + 0x16);
            victim = i;
        }
    }
    i = victim;
    g.cell_build(static_cast<int>(i), first, count, clut, flags & 0x800);
used:
    const unsigned char render_flags = At(kRenderFlags)[0];
    unsigned char* e = At(kCellCache + i * kCellEntry);
    PutWord(e + 0x16, Word(e + 0x16) + 1);
    PutWord(e + 0x14, 1);
    if (!(render_flags & 1)) SetTexture(0, Long(e + 0x24));
    return static_cast<int>(i);
}

void D3dDraw_Inject() {
    if (bof3::WantsShadow("d3d_draw")) d3d_draw::SelfTest();
    BOF3_INJECT(D3d_PrimColor);
    BOF3_INJECT(D3d_SetBlend);
    BOF3_INJECT(D3d_SetShadeMode);
    BOF3_INJECT(D3d_BindTexture);
    BOF3_INJECT(D3d_DrawPolyFT4);
    BOF3_INJECT(D3d_DrawPolyGT4);
    BOF3_INJECT(D3d_DrawLineF2);
    BOF3_INJECT(D3d_DrawLineF4);
    BOF3_INJECT(D3d_DrawTile);
    BOF3_INJECT(D3d_DrawCellSprite);
    BOF3_INJECT(D3d_CellTexture);
}
