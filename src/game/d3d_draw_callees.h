// Internal to d3d_draw.cpp and d3d_draw_fuzz.cpp: every call the Direct3D draw
// handlers and their helpers make, through pointers, so that the start-up fuzz
// can stand recording functions in for them - for Capcom's copies and for ours
// alike. The COM calls go through D3d_Device, which the fuzz replaces with a
// fake (d3d_fuzz.h). docs/d3d-draw.md.
#pragma once

#include <cstdint>

#include "bof3/symbols.gen.h"
#include "game/rdata_consts.h"
// Rebound 2026-09-28 (round eleven's cleanup, docs/round-11-cleanup.md item 2):
// every constant here whose target has a name in symbols.toml reads
// bof3::addr::<Name>. The values are unchanged - the fuzz keys on them.

namespace d3d_draw {

using U = std::uint32_t;

// A bare `ret` (24 callers); every handler calls it twice (the cell sprite
// once) with an argument pushed.
constexpr U kRetOnly = bof3::addr::BareRet;

// Data the handlers read, all Capcom's addresses (docs/d3d-draw.md section 2).
// kScaleX: now D3d_ScaleX (symbols.toml). D3d_ScaleX, float
// kScaleY: now D3d_ScaleY (symbols.toml). D3d_ScaleY, float
// kVertices: now D3d_Vertices (symbols.toml). D3d_Vertices, 4 x D3DTLVERTEX
constexpr ::bof3::image::Va kTexCoords{0x7CA9E0};    // D3d_TexCoords, 256 floats, tc[i] = (i + 0.512) / 256 in game
// kDrawTpage: now Gfx_DrawTpage (symbols.toml). Gfx_DrawTpage, u16: the last draw-mode primitive's tpage word
// kShadeCache: now D3d_ShadeModeCache (symbols.toml). D3d_ShadeModeCache
// kRenderFlags: now Gfx_RenderFlags (symbols.toml). Gfx_RenderFlags, byte: bit 0 the software surfaces
// kPageCache: now Gfx_TexCache (symbols.toml). Gfx_TexCache: 32 pages x 32 entries of 0x18
// kClutRows: now Gfx_ClutRows (symbols.toml). Gfx_ClutRows: u32 generation, u32 pointer, per row
// kCells: now SpriteCell_Table (symbols.toml). SpriteCell_Table: 8-byte records
// kCellCache: now D3d_CellTexCache (symbols.toml). D3d_CellTexCache: 128 entries of 0x28
constexpr U kCellEntry = 0x28;
constexpr U kCellEntries = 0x80;
constexpr rdata::Const kRhwNumerator{0x5C4610}; // float 0.1: rhw = 0.1 / z
constexpr rdata::Const kHalf{0x5C41D8};         // float 0.5: the cell sprite's texel inset

struct Callees {
    void (__cdecl* prim_color)(unsigned r, unsigned g, unsigned b, unsigned code, unsigned mode,
                               unsigned long* diffuse, unsigned long* specular);   // D3d_PrimColor
    long (__cdecl* bind_texture)(unsigned tpage, unsigned clut);                    // D3d_BindTexture
    void (__cdecl* ret_only)(unsigned);                                             // 0x437CC0
    void (__cdecl* set_blend)(unsigned code, unsigned mode);                        // D3d_SetBlend
    void (__cdecl* set_shade)(unsigned mode);                                       // D3d_SetShadeMode
    int (__cdecl* cell_texture)(unsigned first, unsigned count, unsigned clut, unsigned flags);   // D3d_CellTexture
    int (__cdecl* tex_find)(int page, int clut, int mode);                          // Gfx_TexCacheFind
    unsigned long (__cdecl* tex_build)(int page, int clut, int mode);               // D3d_BuildPageTexture
    void (__cdecl* tex_refresh)(int page, int slot);                                // D3d_RefreshPageTexture
    void (__cdecl* cell_build)(int slot, unsigned first, unsigned count, unsigned clut, unsigned flip);   // D3d_BuildCellTexture
    void (__cdecl* cell_refresh)(int slot, unsigned first, unsigned count, unsigned clut);               // D3d_RefreshCellTexture
};

extern const Callees kOriginals;
extern Callees g;

void SelfTest();   // d3d_draw_fuzz.cpp

}  // namespace d3d_draw
