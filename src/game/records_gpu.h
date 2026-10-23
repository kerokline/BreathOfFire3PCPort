// The port's GPU primitive packets and the GTE's short vector, typed
// (docs/typed-records.md section 7; typed-data-stage-c.md section 3 item 5).
// Layout only, as records.h: a field is named where a cited reader names
// it. The packets are built at Gfx_PacketNext and linked into the ordering
// table; Gfx_DrawOTag 0x59EE50 hands each to a handler by its code byte +7
// (docs/d3d-draw.md section 2). They are not libgpu's structs: the port
// keeps a vertex's position as floats and adds its depth
// (docs/psx-library-layer.md section 1, "A primitive is not the PSX
// struct"), so every layout below is the consumer's - the handler that
// draws it - and the setter's that starts it.
#pragma once

#include <cstddef>
#include <cstdint>

namespace records {

// The eight bytes every packet starts with: the list link (Gpu_AddPrim /
// Gpu_LinkPrim; bit 31 kept by AddPrim, psx-library-layer.md 1), the colour
// D3d_PrimColor takes (r, g, b, code: d3d-draw.md 4; Prim_SetShade writes
// +4..+6, prim.cpp) and the GPU code (Gpu_SetPolyFT4 and the other setters;
// bit 1 semi-transparency, Gpu_SetSemiTrans; bit 0 raw texture,
// Gpu_SetShadeTex). Each packet below repeats these five members rather
// than deriving from this struct, so that each stays standard-layout.
struct GpuPrimHead {
    std::uint32_t link;  // +0x00 the ordering table's link (psx-library-layer.md 1)
    std::uint8_t r;  // +0x04 (Prim_SetShade; D3d_PrimColor)
    std::uint8_t g;  // +0x05
    std::uint8_t b;  // +0x06
    std::uint8_t code;  // +0x07 the GPU code, the walk's dispatch: (code & 0xFC) - 0x20 (d3d-draw.md 2)
};

// POLY_FT4, code 0x2C, 0x48 bytes: corners of 0x10 from +8, float x, y, z,
// byte u, v; the CLUT word +0x16, the tpage word +0x26 (D3d_DrawPolyFT4,
// d3d-draw.md 3). Gpu_SetPolyFT4 puts 0.01 in each z (psx_gpu.cpp), the
// transform the depth sz / 16384 (Gte_PrimDepths4_10, psx-library-layer.md
// 3); Prim_SetTexture writes the u, v, the CLUT and the tpage (prim.cpp);
// menu-screens.md 2 saw the floats at +8 / +0xC.
struct GpuPolyFT4 {
    std::uint32_t link;  // +0x00 GpuPrimHead
    std::uint8_t r;  // +0x04
    std::uint8_t g;  // +0x05
    std::uint8_t b;  // +0x06
    std::uint8_t code;  // +0x07
    float x0;  // +0x08
    float y0;  // +0x0C
    float z0;  // +0x10
    std::uint8_t u0;  // +0x14
    std::uint8_t v0;  // +0x15
    std::uint16_t clut;  // +0x16
    float x1;  // +0x18
    float y1;  // +0x1C
    float z1;  // +0x20
    std::uint8_t u1;  // +0x24
    std::uint8_t v1;  // +0x25
    std::uint16_t tpage;  // +0x26
    float x2;  // +0x28
    float y2;  // +0x2C
    float z2;  // +0x30
    std::uint8_t u2;  // +0x34
    std::uint8_t v2;  // +0x35
    unsigned char pad_36[0x2];  // +0x36, +0x37: unread by D3d_DrawPolyFT4
    float x3;  // +0x38
    float y3;  // +0x3C
    float z3;  // +0x40
    std::uint8_t u3;  // +0x44
    std::uint8_t v3;  // +0x45
    unsigned char pad_46[0x2];  // +0x46, +0x47: unread
};

// POLY_GT4, code 0x3C, 0x54 bytes: corners of 0x14 from +4 - r, g, b, pad
// (the first corner's are the head's r, g, b, code), float x, y, z, u, v;
// CLUT +0x16, tpage +0x2A (D3d_DrawPolyGT4, d3d-draw.md 3; Gpu_SetPolyGT4's
// 0.01 at +0x10 and every 0x14, psx_gpu.cpp).
struct GpuPolyGT4 {
    std::uint32_t link;  // +0x00 GpuPrimHead
    std::uint8_t r;  // +0x04
    std::uint8_t g;  // +0x05
    std::uint8_t b;  // +0x06
    std::uint8_t code;  // +0x07
    float x0;  // +0x08
    float y0;  // +0x0C
    float z0;  // +0x10
    std::uint8_t u0;  // +0x14
    std::uint8_t v0;  // +0x15
    std::uint16_t clut;  // +0x16
    std::uint8_t r1;  // +0x18
    std::uint8_t g1;  // +0x19
    std::uint8_t b1;  // +0x1A
    unsigned char pad_1B[0x1];  // +0x1B
    float x1;  // +0x1C
    float y1;  // +0x20
    float z1;  // +0x24
    std::uint8_t u1;  // +0x28
    std::uint8_t v1;  // +0x29
    std::uint16_t tpage;  // +0x2A
    std::uint8_t r2;  // +0x2C
    std::uint8_t g2;  // +0x2D
    std::uint8_t b2;  // +0x2E
    unsigned char pad_2F[0x1];  // +0x2F
    float x2;  // +0x30
    float y2;  // +0x34
    float z2;  // +0x38
    std::uint8_t u2;  // +0x3C
    std::uint8_t v2;  // +0x3D
    unsigned char pad_3E[0x2];  // +0x3E, +0x3F
    std::uint8_t r3;  // +0x40
    std::uint8_t g3;  // +0x41
    std::uint8_t b3;  // +0x42
    unsigned char pad_43[0x1];  // +0x43
    float x3;  // +0x44
    float y3;  // +0x48
    float z3;  // +0x4C
    std::uint8_t u3;  // +0x50
    std::uint8_t v3;  // +0x51
    unsigned char pad_52[0x2];  // +0x52, +0x53
};

// LINE_F2, code 0x40: corners of 0xC from +8, float x, y, z (D3d_DrawLineF2,
// d3d-draw.md 3; Gpu_SetLineF2's 0.01 at +0x10 and +0x1C, psx_gpu.cpp).
struct GpuLineF2 {
    std::uint32_t link;  // +0x00 GpuPrimHead
    std::uint8_t r;  // +0x04
    std::uint8_t g;  // +0x05
    std::uint8_t b;  // +0x06
    std::uint8_t code;  // +0x07
    float x0;  // +0x08
    float y0;  // +0x0C
    float z0;  // +0x10
    float x1;  // +0x14
    float y1;  // +0x18
    float z1;  // +0x1C
};

// LINE_F4, code 0x4C: four corners of 0xC from +8 (D3d_DrawLineF4;
// Gpu_SetLineF4's 0.01 at +0x10..+0x34 every 0xC).
struct GpuLineF4 {
    std::uint32_t link;  // +0x00 GpuPrimHead
    std::uint8_t r;  // +0x04
    std::uint8_t g;  // +0x05
    std::uint8_t b;  // +0x06
    std::uint8_t code;  // +0x07
    float x0;  // +0x08
    float y0;  // +0x0C
    float z0;  // +0x10
    float x1;  // +0x14
    float y1;  // +0x18
    float z1;  // +0x1C
    float x2;  // +0x20
    float y2;  // +0x24
    float z2;  // +0x28
    float x3;  // +0x2C
    float y3;  // +0x30
    float z3;  // +0x34
};

// TILE, code 0x60: float x +8, y +0xC, z +0x10, w +0x14, h +0x18 (D3d_DrawTile,
// d3d-draw.md 3; Gpu_SetTile's 0.01 at +0x10).
struct GpuTile {
    std::uint32_t link;  // +0x00 GpuPrimHead
    std::uint8_t r;  // +0x04
    std::uint8_t g;  // +0x05
    std::uint8_t b;  // +0x06
    std::uint8_t code;  // +0x07
    float x;  // +0x08
    float y;  // +0x0C
    float z;  // +0x10
    float w;  // +0x14
    float h;  // +0x18
};

// SPRT, code 0x64 (and SPRT_8 0x74 / SPRT_16 0x7C, which stop at +0x18):
// float x, y, z, byte u, v, the CLUT word, then SPRT's own u16 w, h
// (sprt_draw.cpp, D3d_DrawSprt and its two siblings; Gpu_SetSprt's 0.01 at
// +0x10; effect_1f.md: UiSprite_Draw's floats at +8 / +0xC).
struct GpuSprt {
    std::uint32_t link;  // +0x00 GpuPrimHead
    std::uint8_t r;  // +0x04
    std::uint8_t g;  // +0x05
    std::uint8_t b;  // +0x06
    std::uint8_t code;  // +0x07
    float x;  // +0x08
    float y;  // +0x0C
    float z;  // +0x10
    std::uint8_t u;  // +0x14
    std::uint8_t v;  // +0x15
    std::uint16_t clut;  // +0x16
    std::uint16_t w;  // +0x18 SPRT only
    std::uint16_t h;  // +0x1A SPRT only
};

// The glyph primitive, code 0x6C (no libgpu primitive; Gpu_SetCode6C):
// corners of 8 from +8, s16 x, y, byte u, v; the CLUT word +0xE and the
// glyph word +0x16 in the first two corners' last word (D3d_DrawGlyph,
// glyph-draw.md 2; magic_c1.md: the character's code at +0x16).
struct GpuGlyph {
    std::uint32_t link;  // +0x00 GpuPrimHead
    std::uint8_t r;  // +0x04
    std::uint8_t g;  // +0x05
    std::uint8_t b;  // +0x06
    std::uint8_t code;  // +0x07
    std::int16_t x0;  // +0x08
    std::int16_t y0;  // +0x0A
    std::uint8_t u0;  // +0x0C
    std::uint8_t v0;  // +0x0D
    std::uint16_t clut;  // +0x0E
    std::int16_t x1;  // +0x10
    std::int16_t y1;  // +0x12
    std::uint8_t u1;  // +0x14
    std::uint8_t v1;  // +0x15
    std::uint16_t glyph;  // +0x16
    std::int16_t x2;  // +0x18
    std::int16_t y2;  // +0x1A
    std::uint8_t u2;  // +0x1C
    std::uint8_t v2;  // +0x1D
    unsigned char pad_1E[0x2];  // +0x1E, +0x1F: unread by D3d_DrawGlyph
    std::int16_t x3;  // +0x20
    std::int16_t y3;  // +0x22
    std::uint8_t u3;  // +0x24
    std::uint8_t v3;  // +0x25
    unsigned char pad_26[0x2];  // +0x26, +0x27
};

// The cell sprite, code 0x84 (the port's own; Gpu_SetCode84, whose one
// caller is Sprite_Draw): float x +8, y +0xC, the sprite's scale +0x10 /
// +0x14, u16 first cell +0x18, count +0x1A, CLUT +0x1C, flags +0x1E (bit
// 0x400 flips x) (D3d_DrawCellSprite, d3d-draw.md 3; sprite-draw-order.md 14:
// the CLUT word at +0x1C, the first index to +0x18 and the count to +0x1A).
struct GpuCellSprite {
    std::uint32_t link;  // +0x00 GpuPrimHead
    std::uint8_t r;  // +0x04
    std::uint8_t g;  // +0x05
    std::uint8_t b;  // +0x06
    std::uint8_t code;  // +0x07
    float x;  // +0x08
    float y;  // +0x0C
    float scale_x;  // +0x10
    float scale_y;  // +0x14
    std::uint16_t first;  // +0x18
    std::uint16_t count;  // +0x1A
    std::uint16_t clut;  // +0x1C
    std::uint16_t flags;  // +0x1E the colour's and the blend's tpage; bit 0x400 flips x
};

// A cell of a cell sprite, 8 bytes, at SpriteCell_Table + index * 8
// (SpriteCell_Add, sprite-draw-order.md 14; D3d_BuildCellTexture's loop,
// tex_cells.cpp DrawCells): its place in the composed texture, its size, and
// where its texels lie in VRAM.
struct SpriteCell {
    std::int16_t x;  // +0x00 left, from the composed texture's centre
    std::int8_t y;  // +0x02 top
    std::uint8_t size;  // +0x03 w = low nibble * 8, h = high nibble * 8
    std::uint16_t tpage;  // +0x04 bits 0..3 the page column, 4 the row, 7..8 the colour mode (0 4-bit), 9 mirrored
    std::uint8_t u;  // +0x06 in texels
    std::uint8_t v;  // +0x07
};

// The draw-mode packet, code 0xE8 (Gpu_SetDrawMode, psx_gpu.cpp:
// 0xE8000000 | tpage at +4, dfe / dtd in bits 0 / 1 of +6, the texture
// window +8; the walk's case 21, d3d-draw.md 2). Its head is a word and a
// dword rather than GpuPrimHead's colour bytes.
struct GpuDrawMode {
    std::uint32_t link;  // +0x00
    std::uint16_t tpage;  // +0x04 Gfx_DrawTpage takes the word (d3d-draw.md 2)
    std::uint8_t mode_flags;  // +0x06 bit 0 dfe (Gfx_DrawEnable), bit 1 dtd
    std::uint8_t code;  // +0x07 0xE8
    std::uint32_t window;  // +0x08 the texture window; Gfx_TexCacheKey from it when non-null (d3d-draw.md 2)
};

// libgte's SVECTOR: three s16 and a pad, eight bytes. Gte_LoadVertex loads
// one, Gte_LoadVertices3 three (psx-library-layer.md 2); Prim_VertexScratch
// 0x9037A0 holds four (symbols.toml: Sprite_AddDrawRecords writes a quad's
// four there at 0, 8, 16, 24 and hands each to Gte_LoadVertex) and the effect kinds'
// Vertices() / Vtx() / Vertex(i) views of it (effect_5e/5f/5g/6a/6b/6d,
// rest_3g: Vertex(i) = Vertices() + 8 * i).
struct SVector {
    std::int16_t x;  // +0x00
    std::int16_t y;  // +0x02
    std::int16_t z;  // +0x04
    std::int16_t pad;  // +0x06 never written by the vertex builders (no store at +6 + 8k in the B1 rows)
};

static_assert(offsetof(GpuPrimHead, link) == 0x0, "GpuPrimHead::link");
static_assert(offsetof(GpuPrimHead, r) == 0x4, "GpuPrimHead::r");
static_assert(offsetof(GpuPrimHead, g) == 0x5, "GpuPrimHead::g");
static_assert(offsetof(GpuPrimHead, b) == 0x6, "GpuPrimHead::b");
static_assert(offsetof(GpuPrimHead, code) == 0x7, "GpuPrimHead::code");
static_assert(sizeof(GpuPrimHead) == 8, "GpuPrimHead stride");

static_assert(offsetof(GpuPolyFT4, link) == 0x0, "GpuPolyFT4::link");
static_assert(offsetof(GpuPolyFT4, r) == 0x4, "GpuPolyFT4::r");
static_assert(offsetof(GpuPolyFT4, g) == 0x5, "GpuPolyFT4::g");
static_assert(offsetof(GpuPolyFT4, b) == 0x6, "GpuPolyFT4::b");
static_assert(offsetof(GpuPolyFT4, code) == 0x7, "GpuPolyFT4::code");
static_assert(offsetof(GpuPolyFT4, x0) == 0x8, "GpuPolyFT4::x0");
static_assert(offsetof(GpuPolyFT4, y0) == 0xC, "GpuPolyFT4::y0");
static_assert(offsetof(GpuPolyFT4, z0) == 0x10, "GpuPolyFT4::z0");
static_assert(offsetof(GpuPolyFT4, u0) == 0x14, "GpuPolyFT4::u0");
static_assert(offsetof(GpuPolyFT4, v0) == 0x15, "GpuPolyFT4::v0");
static_assert(offsetof(GpuPolyFT4, clut) == 0x16, "GpuPolyFT4::clut");
static_assert(offsetof(GpuPolyFT4, x1) == 0x18, "GpuPolyFT4::x1");
static_assert(offsetof(GpuPolyFT4, y1) == 0x1C, "GpuPolyFT4::y1");
static_assert(offsetof(GpuPolyFT4, z1) == 0x20, "GpuPolyFT4::z1");
static_assert(offsetof(GpuPolyFT4, u1) == 0x24, "GpuPolyFT4::u1");
static_assert(offsetof(GpuPolyFT4, v1) == 0x25, "GpuPolyFT4::v1");
static_assert(offsetof(GpuPolyFT4, tpage) == 0x26, "GpuPolyFT4::tpage");
static_assert(offsetof(GpuPolyFT4, x2) == 0x28, "GpuPolyFT4::x2");
static_assert(offsetof(GpuPolyFT4, y2) == 0x2C, "GpuPolyFT4::y2");
static_assert(offsetof(GpuPolyFT4, z2) == 0x30, "GpuPolyFT4::z2");
static_assert(offsetof(GpuPolyFT4, u2) == 0x34, "GpuPolyFT4::u2");
static_assert(offsetof(GpuPolyFT4, v2) == 0x35, "GpuPolyFT4::v2");
static_assert(offsetof(GpuPolyFT4, x3) == 0x38, "GpuPolyFT4::x3");
static_assert(offsetof(GpuPolyFT4, y3) == 0x3C, "GpuPolyFT4::y3");
static_assert(offsetof(GpuPolyFT4, z3) == 0x40, "GpuPolyFT4::z3");
static_assert(offsetof(GpuPolyFT4, u3) == 0x44, "GpuPolyFT4::u3");
static_assert(offsetof(GpuPolyFT4, v3) == 0x45, "GpuPolyFT4::v3");
static_assert(sizeof(GpuPolyFT4) == 0x48, "GpuPolyFT4 stride");

static_assert(offsetof(GpuPolyGT4, link) == 0x0, "GpuPolyGT4::link");
static_assert(offsetof(GpuPolyGT4, r) == 0x4, "GpuPolyGT4::r");
static_assert(offsetof(GpuPolyGT4, g) == 0x5, "GpuPolyGT4::g");
static_assert(offsetof(GpuPolyGT4, b) == 0x6, "GpuPolyGT4::b");
static_assert(offsetof(GpuPolyGT4, code) == 0x7, "GpuPolyGT4::code");
static_assert(offsetof(GpuPolyGT4, x0) == 0x8, "GpuPolyGT4::x0");
static_assert(offsetof(GpuPolyGT4, y0) == 0xC, "GpuPolyGT4::y0");
static_assert(offsetof(GpuPolyGT4, z0) == 0x10, "GpuPolyGT4::z0");
static_assert(offsetof(GpuPolyGT4, u0) == 0x14, "GpuPolyGT4::u0");
static_assert(offsetof(GpuPolyGT4, v0) == 0x15, "GpuPolyGT4::v0");
static_assert(offsetof(GpuPolyGT4, clut) == 0x16, "GpuPolyGT4::clut");
static_assert(offsetof(GpuPolyGT4, r1) == 0x18, "GpuPolyGT4::r1");
static_assert(offsetof(GpuPolyGT4, g1) == 0x19, "GpuPolyGT4::g1");
static_assert(offsetof(GpuPolyGT4, b1) == 0x1A, "GpuPolyGT4::b1");
static_assert(offsetof(GpuPolyGT4, x1) == 0x1C, "GpuPolyGT4::x1");
static_assert(offsetof(GpuPolyGT4, y1) == 0x20, "GpuPolyGT4::y1");
static_assert(offsetof(GpuPolyGT4, z1) == 0x24, "GpuPolyGT4::z1");
static_assert(offsetof(GpuPolyGT4, u1) == 0x28, "GpuPolyGT4::u1");
static_assert(offsetof(GpuPolyGT4, v1) == 0x29, "GpuPolyGT4::v1");
static_assert(offsetof(GpuPolyGT4, tpage) == 0x2A, "GpuPolyGT4::tpage");
static_assert(offsetof(GpuPolyGT4, r2) == 0x2C, "GpuPolyGT4::r2");
static_assert(offsetof(GpuPolyGT4, g2) == 0x2D, "GpuPolyGT4::g2");
static_assert(offsetof(GpuPolyGT4, b2) == 0x2E, "GpuPolyGT4::b2");
static_assert(offsetof(GpuPolyGT4, x2) == 0x30, "GpuPolyGT4::x2");
static_assert(offsetof(GpuPolyGT4, y2) == 0x34, "GpuPolyGT4::y2");
static_assert(offsetof(GpuPolyGT4, z2) == 0x38, "GpuPolyGT4::z2");
static_assert(offsetof(GpuPolyGT4, u2) == 0x3C, "GpuPolyGT4::u2");
static_assert(offsetof(GpuPolyGT4, v2) == 0x3D, "GpuPolyGT4::v2");
static_assert(offsetof(GpuPolyGT4, r3) == 0x40, "GpuPolyGT4::r3");
static_assert(offsetof(GpuPolyGT4, g3) == 0x41, "GpuPolyGT4::g3");
static_assert(offsetof(GpuPolyGT4, b3) == 0x42, "GpuPolyGT4::b3");
static_assert(offsetof(GpuPolyGT4, x3) == 0x44, "GpuPolyGT4::x3");
static_assert(offsetof(GpuPolyGT4, y3) == 0x48, "GpuPolyGT4::y3");
static_assert(offsetof(GpuPolyGT4, z3) == 0x4C, "GpuPolyGT4::z3");
static_assert(offsetof(GpuPolyGT4, u3) == 0x50, "GpuPolyGT4::u3");
static_assert(offsetof(GpuPolyGT4, v3) == 0x51, "GpuPolyGT4::v3");
static_assert(sizeof(GpuPolyGT4) == 0x54, "GpuPolyGT4 stride");

static_assert(offsetof(GpuLineF2, link) == 0x0, "GpuLineF2::link");
static_assert(offsetof(GpuLineF2, r) == 0x4, "GpuLineF2::r");
static_assert(offsetof(GpuLineF2, g) == 0x5, "GpuLineF2::g");
static_assert(offsetof(GpuLineF2, b) == 0x6, "GpuLineF2::b");
static_assert(offsetof(GpuLineF2, code) == 0x7, "GpuLineF2::code");
static_assert(offsetof(GpuLineF2, x0) == 0x8, "GpuLineF2::x0");
static_assert(offsetof(GpuLineF2, y0) == 0xC, "GpuLineF2::y0");
static_assert(offsetof(GpuLineF2, z0) == 0x10, "GpuLineF2::z0");
static_assert(offsetof(GpuLineF2, x1) == 0x14, "GpuLineF2::x1");
static_assert(offsetof(GpuLineF2, y1) == 0x18, "GpuLineF2::y1");
static_assert(offsetof(GpuLineF2, z1) == 0x1C, "GpuLineF2::z1");
static_assert(sizeof(GpuLineF2) == 0x20, "GpuLineF2 stride");

static_assert(offsetof(GpuLineF4, link) == 0x0, "GpuLineF4::link");
static_assert(offsetof(GpuLineF4, r) == 0x4, "GpuLineF4::r");
static_assert(offsetof(GpuLineF4, g) == 0x5, "GpuLineF4::g");
static_assert(offsetof(GpuLineF4, b) == 0x6, "GpuLineF4::b");
static_assert(offsetof(GpuLineF4, code) == 0x7, "GpuLineF4::code");
static_assert(offsetof(GpuLineF4, x0) == 0x8, "GpuLineF4::x0");
static_assert(offsetof(GpuLineF4, y0) == 0xC, "GpuLineF4::y0");
static_assert(offsetof(GpuLineF4, z0) == 0x10, "GpuLineF4::z0");
static_assert(offsetof(GpuLineF4, x1) == 0x14, "GpuLineF4::x1");
static_assert(offsetof(GpuLineF4, y1) == 0x18, "GpuLineF4::y1");
static_assert(offsetof(GpuLineF4, z1) == 0x1C, "GpuLineF4::z1");
static_assert(offsetof(GpuLineF4, x2) == 0x20, "GpuLineF4::x2");
static_assert(offsetof(GpuLineF4, y2) == 0x24, "GpuLineF4::y2");
static_assert(offsetof(GpuLineF4, z2) == 0x28, "GpuLineF4::z2");
static_assert(offsetof(GpuLineF4, x3) == 0x2C, "GpuLineF4::x3");
static_assert(offsetof(GpuLineF4, y3) == 0x30, "GpuLineF4::y3");
static_assert(offsetof(GpuLineF4, z3) == 0x34, "GpuLineF4::z3");
static_assert(sizeof(GpuLineF4) == 0x38, "GpuLineF4 stride");

static_assert(offsetof(GpuTile, link) == 0x0, "GpuTile::link");
static_assert(offsetof(GpuTile, r) == 0x4, "GpuTile::r");
static_assert(offsetof(GpuTile, g) == 0x5, "GpuTile::g");
static_assert(offsetof(GpuTile, b) == 0x6, "GpuTile::b");
static_assert(offsetof(GpuTile, code) == 0x7, "GpuTile::code");
static_assert(offsetof(GpuTile, x) == 0x8, "GpuTile::x");
static_assert(offsetof(GpuTile, y) == 0xC, "GpuTile::y");
static_assert(offsetof(GpuTile, z) == 0x10, "GpuTile::z");
static_assert(offsetof(GpuTile, w) == 0x14, "GpuTile::w");
static_assert(offsetof(GpuTile, h) == 0x18, "GpuTile::h");
static_assert(sizeof(GpuTile) == 0x1C, "GpuTile stride");

static_assert(offsetof(GpuSprt, link) == 0x0, "GpuSprt::link");
static_assert(offsetof(GpuSprt, r) == 0x4, "GpuSprt::r");
static_assert(offsetof(GpuSprt, g) == 0x5, "GpuSprt::g");
static_assert(offsetof(GpuSprt, b) == 0x6, "GpuSprt::b");
static_assert(offsetof(GpuSprt, code) == 0x7, "GpuSprt::code");
static_assert(offsetof(GpuSprt, x) == 0x8, "GpuSprt::x");
static_assert(offsetof(GpuSprt, y) == 0xC, "GpuSprt::y");
static_assert(offsetof(GpuSprt, z) == 0x10, "GpuSprt::z");
static_assert(offsetof(GpuSprt, u) == 0x14, "GpuSprt::u");
static_assert(offsetof(GpuSprt, v) == 0x15, "GpuSprt::v");
static_assert(offsetof(GpuSprt, clut) == 0x16, "GpuSprt::clut");
static_assert(offsetof(GpuSprt, w) == 0x18, "GpuSprt::w");
static_assert(offsetof(GpuSprt, h) == 0x1A, "GpuSprt::h");
static_assert(sizeof(GpuSprt) == 0x1C, "GpuSprt stride");

static_assert(offsetof(GpuGlyph, link) == 0x0, "GpuGlyph::link");
static_assert(offsetof(GpuGlyph, r) == 0x4, "GpuGlyph::r");
static_assert(offsetof(GpuGlyph, g) == 0x5, "GpuGlyph::g");
static_assert(offsetof(GpuGlyph, b) == 0x6, "GpuGlyph::b");
static_assert(offsetof(GpuGlyph, code) == 0x7, "GpuGlyph::code");
static_assert(offsetof(GpuGlyph, x0) == 0x8, "GpuGlyph::x0");
static_assert(offsetof(GpuGlyph, y0) == 0xA, "GpuGlyph::y0");
static_assert(offsetof(GpuGlyph, u0) == 0xC, "GpuGlyph::u0");
static_assert(offsetof(GpuGlyph, v0) == 0xD, "GpuGlyph::v0");
static_assert(offsetof(GpuGlyph, clut) == 0xE, "GpuGlyph::clut");
static_assert(offsetof(GpuGlyph, x1) == 0x10, "GpuGlyph::x1");
static_assert(offsetof(GpuGlyph, y1) == 0x12, "GpuGlyph::y1");
static_assert(offsetof(GpuGlyph, u1) == 0x14, "GpuGlyph::u1");
static_assert(offsetof(GpuGlyph, v1) == 0x15, "GpuGlyph::v1");
static_assert(offsetof(GpuGlyph, glyph) == 0x16, "GpuGlyph::glyph");
static_assert(offsetof(GpuGlyph, x2) == 0x18, "GpuGlyph::x2");
static_assert(offsetof(GpuGlyph, y2) == 0x1A, "GpuGlyph::y2");
static_assert(offsetof(GpuGlyph, u2) == 0x1C, "GpuGlyph::u2");
static_assert(offsetof(GpuGlyph, v2) == 0x1D, "GpuGlyph::v2");
static_assert(offsetof(GpuGlyph, x3) == 0x20, "GpuGlyph::x3");
static_assert(offsetof(GpuGlyph, y3) == 0x22, "GpuGlyph::y3");
static_assert(offsetof(GpuGlyph, u3) == 0x24, "GpuGlyph::u3");
static_assert(offsetof(GpuGlyph, v3) == 0x25, "GpuGlyph::v3");
static_assert(sizeof(GpuGlyph) == 0x28, "GpuGlyph stride");

static_assert(offsetof(GpuCellSprite, link) == 0x0, "GpuCellSprite::link");
static_assert(offsetof(GpuCellSprite, r) == 0x4, "GpuCellSprite::r");
static_assert(offsetof(GpuCellSprite, g) == 0x5, "GpuCellSprite::g");
static_assert(offsetof(GpuCellSprite, b) == 0x6, "GpuCellSprite::b");
static_assert(offsetof(GpuCellSprite, code) == 0x7, "GpuCellSprite::code");
static_assert(offsetof(GpuCellSprite, x) == 0x8, "GpuCellSprite::x");
static_assert(offsetof(GpuCellSprite, y) == 0xC, "GpuCellSprite::y");
static_assert(offsetof(GpuCellSprite, scale_x) == 0x10, "GpuCellSprite::scale_x");
static_assert(offsetof(GpuCellSprite, scale_y) == 0x14, "GpuCellSprite::scale_y");
static_assert(offsetof(GpuCellSprite, first) == 0x18, "GpuCellSprite::first");
static_assert(offsetof(GpuCellSprite, count) == 0x1A, "GpuCellSprite::count");
static_assert(offsetof(GpuCellSprite, clut) == 0x1C, "GpuCellSprite::clut");
static_assert(offsetof(GpuCellSprite, flags) == 0x1E, "GpuCellSprite::flags");
static_assert(sizeof(GpuCellSprite) == 0x20, "GpuCellSprite stride");

static_assert(offsetof(SpriteCell, x) == 0x0, "SpriteCell::x");
static_assert(offsetof(SpriteCell, y) == 0x2, "SpriteCell::y");
static_assert(offsetof(SpriteCell, size) == 0x3, "SpriteCell::size");
static_assert(offsetof(SpriteCell, tpage) == 0x4, "SpriteCell::tpage");
static_assert(offsetof(SpriteCell, u) == 0x6, "SpriteCell::u");
static_assert(offsetof(SpriteCell, v) == 0x7, "SpriteCell::v");
static_assert(sizeof(SpriteCell) == 8, "SpriteCell stride");

static_assert(offsetof(GpuDrawMode, link) == 0x0, "GpuDrawMode::link");
static_assert(offsetof(GpuDrawMode, tpage) == 0x4, "GpuDrawMode::tpage");
static_assert(offsetof(GpuDrawMode, mode_flags) == 0x6, "GpuDrawMode::mode_flags");
static_assert(offsetof(GpuDrawMode, code) == 0x7, "GpuDrawMode::code");
static_assert(offsetof(GpuDrawMode, window) == 0x8, "GpuDrawMode::window");
static_assert(sizeof(GpuDrawMode) == 0xC, "GpuDrawMode stride");

static_assert(offsetof(SVector, x) == 0x0, "SVector::x");
static_assert(offsetof(SVector, y) == 0x2, "SVector::y");
static_assert(offsetof(SVector, z) == 0x4, "SVector::z");
static_assert(offsetof(SVector, pad) == 0x6, "SVector::pad");
static_assert(sizeof(SVector) == 8, "SVector stride");

}  // namespace records
