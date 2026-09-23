// Internal to glyph_draw.cpp and glyph_draw_fuzz.cpp: every call the glyph
// handler and its texture lookup make, through pointers, so that the start-up
// fuzz can stand recording functions in for them - for Capcom's copies and for
// ours alike. The COM calls go through D3d_Device, which the fuzz replaces
// with a fake (d3d_fuzz.h). docs/glyph-draw.md.
#pragma once

#include <cstdint>

namespace glyph_draw {

// A bare `ret` (24 callers), called twice by the handler with a 1 pushed.
constexpr std::uint32_t kRetOnly = 0x437CC0;

// The glyph texture cache: 128 entries of 0x14 bytes from 0x7C9F50
// (Font_TexCache): u16 glyph +0, u16 clut +2, u32 the CLUT row's generation
// +4, the DirectDraw surface +8, the Direct3D texture +0xC, u16 in use this
// frame +0x10. The draw 0x59EE50 clears every +0x10 after EndScene.
constexpr std::uint32_t kCache = 0x7C9F50;
constexpr std::uint32_t kEntry = 0x14;
constexpr std::uint32_t kEntries = 0x80;
constexpr std::uint32_t kClutRows = 0x6C2A40;     // Gfx_ClutRows: u32 generation, u32 pointer, per row
constexpr std::uint32_t kRenderFlags = 0x6C3A4C;  // Gfx_RenderFlags: bit 0 the software surfaces
constexpr std::uint32_t kInv32 = 0x5C4618;        // a double, 1/32 - the glyph texture's 32 texels

struct Callees {
    void (__cdecl* prim_color)(unsigned r, unsigned g, unsigned b, unsigned code, unsigned mode,
                               unsigned long* diffuse, unsigned long* specular);   // D3d_PrimColor
    int (__cdecl* glyph_texture)(unsigned glyph, unsigned clut);                    // Font_GlyphTexture
    void (__cdecl* ret_only)(unsigned);                                             // 0x437CC0
    void (__cdecl* set_blend)(unsigned code, unsigned mode);                        // D3d_SetBlend
    void (__cdecl* set_shade)(unsigned mode);                                       // D3d_SetShadeMode
    void (__cdecl* build_texture)(int slot, unsigned glyph, unsigned clut);         // Font_BuildGlyphTexture
};

extern const Callees kOriginals;
extern Callees g;

// DIVERGENCE DIV-0025: what the handler adds to 2u and 2v before the 1/32 -
// 0 is Capcom's (the texel edge), 0.5 the texel centre. 0 until
// GlyphDraw_Inject sets it; the fuzz sets it itself.
extern float g_texel_inset;

void SelfTest();   // glyph_draw_fuzz.cpp

}  // namespace glyph_draw
