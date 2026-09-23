// The three Direct3D sprite handlers, read to the last instruction with
// capstone against bof3/BOF3.exe (docs/sprt-draw.md). All are the PC port's
// own: the PlayStation drew SPRT, SPRT_8 and SPRT_16 on its GPU, so there is
// no PSX twin to read them against.
//
//   D3d_DrawSprt    0x5A2300..0x5A2510 (0x211)  code 0x64, SPRT
//   D3d_DrawSprt8   0x5A2520..0x5A270A (0x1EB)  code 0x74, SPRT_8
//   D3d_DrawSprt16  0x5A2710..0x5A28FA (0x1EB)  code 0x7C, SPRT_16
//
// Faithful but for DIVERGENCE DIV-0010, the far texture edge. The arithmetic
// is x87 in the original and x87 here, instruction for instruction (the X87*
// helpers below), as in d3d_draw.cpp: each fadd, fmul and fdiv rounds to the
// control word's precision and a float that passes through fld / fstp comes
// out with a signalling NaN quieted.
//
// DIV-0010: a sprite's far texture edge is one texel short.
//
// The handlers read both texture edges of their quad from one float table,
// D3d_TexCoords 0x7CA9E0: tc[i] = (i + 0.512) / 256, 256 entries, built by
// 0x5A5160. The near edge is tc[u]. The far edge is tc[u + w - 1] (operand
// 0x7CA9DC, index u + w), tc[u + 7] (0x7CA9FC) and tc[u + 15] (0x7CAA1C) -
// while the quad is the full w, 8 or 16 pixels wide. So w - 1 texels are
// stretched over w pixels, and at the 2x scale the last row and column of
// every sprite get one screen pixel instead of two: the clipped numerals of
// known-defects.md D1.
//
// What the far edge has to be. The port draws with bilinear filtering (seen:
// the title logo, smooth at 2x), and its sprites are cells of shared texture
// pages, so a sample past the centre of the last texel blends in a
// neighbouring cell. That is what the original's numbers are for: near edge
// at the centre of texel u, far edge at the centre of texel u + w - 1. The
// slip is WHERE the far value is reached: a vertex value is reached at the
// vertex, one pixel past the last one drawn, so the last pixel samples only
// u + 7.07 of 7.5 for an 8 pixel sprite at 2x - the last texel gets 0.7 of a
// screen row where the first gets 1.6. (First try, 2026-09-20: far edge
// u + w with a small inset. It drew seams through the title logo - the
// neighbour blending in. Kept here because it is the obvious fix and wrong.)
//
// Ours: the last PIXEL samples the centre of the last texel. With the
// rasteriser's pixel centres on whole numbers (the Direct3D 6 rule, and what
// the original's near edge assumes), that is a far vertex value of
//
//   near + (w - 1) * n / (n - 1),   n = w * scale screen pixels
//
// which for scale 2 is u + w - 0.5 + (w - 1) / (2w - 1) + 0.012: the last term
// runs from 0.4667 at w = 8 to 0.5 for a wide sprite. One table cannot hold a
// function of u and w both, so ours uses the w = 8 value throughout,
// g_far[j] = (j - 1/30 + 0.012) / 256 indexed by j = u + w: exact for the 8
// pixel font, short of the last texel's centre by under 0.03 of a texel for
// anything wider (never past it, so never a seam), and past it by 0.03 for a
// sprite 4 wide. The scale is 2.0 in every mode seen (0x7C9F4C / 0x7C9F48);
// at another scale this is still between the original and exact.
//
// The near edge is the original's, untouched, as is the table at 0x7CA9E0.
//
// The switch. The handlers read the far edge at g_far_base + 4j. That base is
// Capcom's 0x7CA9DC until SprtDraw_Inject patches it to g_far through
// PatchBytes under the name "SpriteFarEdge" - so BOF3X_ORIGINAL=SpriteFarEdge
// runs our three with Capcom's texture edges, and BOF3X_ORIGINAL=D3d_DrawSprt
// (or 8, 16) runs Capcom's handler itself, as it always has.
//
// An edge the original never defined: its table has 256 entries and SPRT
// indexes it with u + w - 1, a 16-bit w, unchecked - past 255 it reads
// whatever follows (docs/known-defects.md D37, kept). Until 2026-09-23 ours
// was a table of 1,024 read the same way, unchecked, so past 1,024 it read our
// own dll's memory; g_far now has an entry for every index u + w can make.
#include "game/sprt_draw.h"

#include <bit>
#include <cstdint>
#include <cstring>

#include "bof3/symbols.gen.h"
#include "game/sprt_draw_callees.h"
#include "hook/detour.h"
#include "hook/log.h"

namespace sprt_draw {

const Callees kOriginals = {
    D3d_PrimColor,
    D3d_BindTexture,
    reinterpret_cast<void (__cdecl*)(unsigned)>(static_cast<std::uintptr_t>(kRetOnly)),
    D3d_SetBlend,
    D3d_SetShadeMode,
};
Callees g = kOriginals;

float g_far[kFarEntries];
U g_far_base = kCapcomFarBase;

// Exactly the expression of the re-aimed copies that carried DIV-0010 from
// 2026-09-20 (src/game/gfx_sprite_uv.cpp, now gone): (float(j) + inset) / 256
// in float. The sum is exact before its one rounding under any precision
// (j has 17 bits, the inset 24 and an exponent of -6), and / 256 is exact, so
// the table does not depend on the x87 control word or on SSE against x87.
//
// Pinned, because the fuzz cannot see the table's values (ours and the
// re-aimed copies read the same one): the inset's bits, and an FNV-1a of the
// first 1,024 entries' bytes - the whole of the table the copies had - both
// computed independently in numpy float32 on 2026-09-23.
void FillFarTable() {
    constexpr float kFarInset = 0.012f - 1.0f / 30.0f;
    static_assert(std::bit_cast<U>(kFarInset) == 0xBCAEC33Fu, "DIV-0010's inset is not the one of 2026-09-20");
    for (U j = 0; j < kFarEntries; ++j) g_far[j] = (static_cast<float>(j) + kFarInset) / 256.0f;
    U hash = 0x811C9DC5u;
    const auto* bytes = reinterpret_cast<const unsigned char*>(g_far);
    for (U i = 0; i < 1024 * 4; ++i) hash = (hash ^ bytes[i]) * 0x01000193u;
    if (hash != 0xF06E2C4Bu) bof3::Fatal("DIV-0010: the far-edge table is not the one of 2026-09-20 (FNV %08X)", hash);
}

namespace {

unsigned char* At(U address) { return reinterpret_cast<unsigned char*>(static_cast<std::uintptr_t>(address)); }
U Word(const unsigned char* p) {
    std::uint16_t v;
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
// fld a; fadd b; fmul c; fstp - (a + b) * c, one rounding after each
inline float X87AddMul(float a, float b, float c) {
    float r;
    __asm__ volatile("flds %1\n\tfadds %2\n\tfmuls %3\n\tfstps %0" : "=m"(r) : "m"(a), "m"(b), "m"(c) : "st");
    return r;
}
// fild i; fadd b; fmul c; fstp - (i + b) * c
inline float X87IntAddMul(std::int32_t i, float b, float c) {
    float r;
    __asm__ volatile("fildl %1\n\tfadds %2\n\tfmuls %3\n\tfstps %0" : "=m"(r) : "m"(i), "m"(b), "m"(c) : "st");
    return r;
}

// D3d_Device, read afresh before the COM call as the original does.
void** Device() { return *reinterpret_cast<void** volatile*>(At(0x7CC350)); }
void* Method(void** device, U offset) { return (*reinterpret_cast<void***>(device))[offset / 4]; }
using Com6 = long(__stdcall*)(void*, U, U, U, U, U);

unsigned char* Vertex(U i) { return At(kVertices + i * 0x20); }

// The body all three share. `size_at` is 0 for SPRT - the size is the
// primitive's u16 w +0x18 and h +0x1A, pushed through fild - or the address of
// the fixed size's float, 8.0 or 16.0, with `extent` 8 or 16 its far index.
//
// The primitive (Menu_DrawPiece 0x57D860 builds a SPRT, the 8 x 8 font 0x517090
// a SPRT_8): r, g, b, code at +4..+7; float x +8, y +0xC, z +0x10; u8 u +0x14,
// v +0x15; u16 CLUT +0x16; SPRT only: u16 w +0x18, h +0x1A.
//
// Corners, in the strip order: (x, y), (x + w, y), (x, y + h), (x + w, y + h),
// each times the scale; texture (near u, near v), (far u, near v), (near u,
// far v), (far u, far v). Every one of the 0x80 bytes is written. The far
// edges' sums are made on the x87 before the multiply, and the copies the
// original makes by `mov` (the second corner's sy, the fourth's sx, the
// repeated texture coordinates) are the same bits here.
//
// Every read of the primitive, the scales and the tables is between the
// colour helper and the texture bind, as in the original; the code byte and
// Gfx_DrawTpage are read again for the blend after the texture and the two
// bare rets - kept, since a callee could change either.
long DrawSprite(const unsigned char* prim, U size_at, U extent) {
    unsigned long diffuse, specular;
    g.prim_color(prim[4], prim[5], prim[6], prim[7], Long(At(kDrawTpage)) & 0xFFFF, &diffuse, &specular);

    const float x = Float(prim + 8), y = Float(prim + 0xC), z = Float(prim + 0x10);
    const U u = prim[0x14], v = prim[0x15];
    const float left = X87Mul(FloatAt(kScaleX), x);
    const float top = X87Mul(FloatAt(kScaleY), y);
    const float near_u = X87Pass(FloatAt(kTexCoords + u * 4)), near_v = X87Pass(FloatAt(kTexCoords + v * 4));
    float right, bottom;
    U far_j_u, far_j_v;
    if (size_at == 0) {
        // u16, zero-extended: a w of 0x8000 or more is a wide sprite, never a
        // negative one.
        const U w = Word(prim + 0x18), h = Word(prim + 0x1A);
        right = X87IntAddMul(static_cast<std::int32_t>(w), x, FloatAt(kScaleX));
        bottom = X87IntAddMul(static_cast<std::int32_t>(h), y, FloatAt(kScaleY));
        far_j_u = u + w;
        far_j_v = v + h;
    } else {
        right = X87AddMul(x, FloatAt(size_at), FloatAt(kScaleX));
        bottom = X87AddMul(y, FloatAt(size_at), FloatAt(kScaleY));
        far_j_u = u + extent;
        far_j_v = v + extent;
    }
    // The far edge: Capcom's tc[j - 1], or DIV-0010's g_far[j]. Not bounded,
    // in either (D37): the address is 32-bit arithmetic, as the original's
    // [reg * 4 + disp32].
    const U base = g_far_base;
    const float far_u = X87Pass(FloatAt(base + far_j_u * 4));
    const float far_v = X87Pass(FloatAt(base + far_j_v * 4));
    const float sz = X87Pass(z);
    const float rhw = X87Div(FloatAt(kRhwNumerator), z);

    const float xs[4] = {left, right, left, right}, ys[4] = {top, top, bottom, bottom};
    const float us[4] = {near_u, far_u, near_u, far_u}, vs[4] = {near_v, near_v, far_v, far_v};
    for (U i = 0; i < 4; ++i) {
        unsigned char* out = Vertex(i);
        PutFloat(out + 0x00, xs[i]);
        PutFloat(out + 0x04, ys[i]);
        PutFloat(out + 0x08, sz);
        PutFloat(out + 0x0C, rhw);
        PutLong(out + 0x10, static_cast<U>(diffuse));
        PutLong(out + 0x14, static_cast<U>(specular));
        PutFloat(out + 0x18, us[i]);
        PutFloat(out + 0x1C, vs[i]);
    }

    g.bind_texture(Long(At(kDrawTpage)) & 0xFFFF, Word(prim + 0x16));
    g.ret_only(1);
    g.ret_only(1);
    g.set_blend(prim[7], Long(At(kDrawTpage)) & 0xFFFF);
    g.set_shade(1);   // flat
    // DrawPrimitive(D3DPT_TRIANGLESTRIP, D3DFVF_TLVERTEX, D3d_Vertices, 4, 0);
    // its result is the handler's (the draw 0x59EE50 does not read it).
    void** device = Device();
    return reinterpret_cast<Com6>(Method(device, 0x70))(device, 5, 0x1C4, kVertices, 4, 0);
    // Not kept: the original lets the colour helper write the diffuse into
    // the caller's pushed `prim` slot, which the caller pops unread, and uses
    // a local as fild's operand.
}

}  // namespace
}  // namespace sprt_draw

// 0x5A2300, code 0x64 (SPRT): a sprite of any size, w and h from the primitive.
long D3d_DrawSprt(const unsigned char* prim) { return sprt_draw::DrawSprite(prim, 0, 0); }

// 0x5A2520, code 0x74 (SPRT_8): an 8 x 8 sprite - x + 8.0 from 0x5C41CC.
long D3d_DrawSprt8(const unsigned char* prim) { return sprt_draw::DrawSprite(prim, sprt_draw::kEight, 8); }

// 0x5A2710, code 0x7C (SPRT_16): a 16 x 16 sprite - x + 16.0 from 0x5C41D0.
long D3d_DrawSprt16(const unsigned char* prim) { return sprt_draw::DrawSprite(prim, sprt_draw::kSixteen, 16); }

void SprtDraw_Inject() {
    using namespace sprt_draw;
    // The table first: the fuzz's DIV-0010 half reads it.
    FillFarTable();
    // The copies are of Capcom's own bytes: nothing patches the three but
    // BOF3_INJECT below (their callers reach them through the draw's jump
    // table), so this module's place in inject_all.cpp does not matter.
    if (bof3::WantsShadow("sprt_draw")) SelfTest();
    BOF3_INJECT(D3d_DrawSprt);
    BOF3_INJECT(D3d_DrawSprt8);
    BOF3_INJECT(D3d_DrawSprt16);

    // DIVERGENCE DIV-0010: the far texture edge from our table. Patched into
    // our own base so that it has a name of its own for BOF3X_ORIGINAL.
    const U was = kCapcomFarBase;
    const U is = static_cast<U>(reinterpret_cast<std::uintptr_t>(g_far));
    std::uint8_t was_bytes[4], is_bytes[4];
    std::memcpy(was_bytes, &was, 4);
    std::memcpy(is_bytes, &is, 4);
    bof3::PatchBytes("SpriteFarEdge", static_cast<U>(reinterpret_cast<std::uintptr_t>(&g_far_base)), was_bytes,
                     is_bytes, 4);
    bof3::Log("DIV-0010    our sprite handlers' far texture edge: %s", g_far_base == kCapcomFarBase
                                                                          ? "Capcom's, tc[u + w - 1]"
                                                                          : "the last pixel on the last texel's centre");
}
