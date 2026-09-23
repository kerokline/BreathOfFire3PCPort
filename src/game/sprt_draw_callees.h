// Internal to sprt_draw.cpp and sprt_draw_fuzz.cpp: every call the three
// Direct3D sprite handlers make, through pointers, so that the start-up fuzz
// can stand recording functions in for them - for Capcom's copies and for ours
// alike - and the far texture edge's table base, which is DIVERGENCE DIV-0010.
// The COM call goes through D3d_Device, which the fuzz replaces with a fake
// (d3d_fuzz.h). docs/sprt-draw.md.
#pragma once

#include <cstdint>

namespace sprt_draw {

using U = std::uint32_t;

// A bare `ret` (24 callers); each handler calls it twice with a 1 pushed.
constexpr U kRetOnly = 0x437CC0;

// Data the handlers read, all Capcom's addresses (docs/sprt-draw.md section 2).
constexpr U kScaleX = 0x7C9F4C;        // D3d_ScaleX, float
constexpr U kScaleY = 0x7C9F48;        // D3d_ScaleY, float
constexpr U kVertices = 0x7CA958;      // D3d_Vertices, 4 x D3DTLVERTEX
constexpr U kTexCoords = 0x7CA9E0;     // D3d_TexCoords, 256 floats, tc[i] = (i + 0.512) / 256 in game
constexpr U kDrawTpage = 0x7DED14;     // Gfx_DrawTpage, u16
constexpr U kRhwNumerator = 0x5C4610;  // float 0.1: rhw = 0.1 / z
constexpr U kEight = 0x5C41CC;         // float 8.0: D3d_DrawSprt8's size
constexpr U kSixteen = 0x5C41D0;       // float 16.0: D3d_DrawSprt16's size

// The far texture edge is read at base + 4 * j, j = u + w (u + 8, u + 16 for
// the fixed sizes). Capcom's base is one entry before D3d_TexCoords - the
// operand 0x7CA9DC of D3d_DrawSprt; D3d_DrawSprt8's 0x7CA9FC and
// D3d_DrawSprt16's 0x7CAA1C are the same base with 8 and 16 entries added -
// so the far edge is tc[u + w - 1].
constexpr U kCapcomFarBase = 0x7CA9DC;

// DIVERGENCE DIV-0010's table: g_far[j] = (j - 1/30 + 0.012) / 256, one
// entry for every j a u8 plus a u16 can make, so no index leaves it.
constexpr U kFarEntries = 0x10100;
extern float g_far[kFarEntries];
void FillFarTable();

// The base the handlers read the far edge from: kCapcomFarBase until
// SprtDraw_Inject patches it to g_far (under the name "SpriteFarEdge"); the
// fuzz sets it itself.
extern U g_far_base;

struct Callees {
    void (__cdecl* prim_color)(unsigned r, unsigned g, unsigned b, unsigned code, unsigned mode,
                               unsigned long* diffuse, unsigned long* specular);   // D3d_PrimColor
    long (__cdecl* bind_texture)(unsigned tpage, unsigned clut);                    // D3d_BindTexture
    void (__cdecl* ret_only)(unsigned);                                             // 0x437CC0
    void (__cdecl* set_blend)(unsigned code, unsigned mode);                        // D3d_SetBlend
    void (__cdecl* set_shade)(unsigned mode);                                       // D3d_SetShadeMode
};

extern const Callees kOriginals;
extern Callees g;

void SelfTest();   // sprt_draw_fuzz.cpp

}  // namespace sprt_draw
