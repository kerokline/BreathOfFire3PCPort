// Internal to tex_cells.cpp and tex_cells_fuzz.cpp: the calls the glyph and
// cell texture builders make that the start-up fuzz stands recorders in for,
// through pointers - the five texel unpackers and Gfx_ClutPixels. The surface
// helpers and this module's own three helpers are called by name; the COM
// calls go through Dd_DirectDraw, the surfaces and D3d_Device, which the fuzz
// replaces with fakes (ddraw_fuzz.h and a device of its own).
// docs/tex-cells.md.
#pragma once

#include <cstdint>

namespace tex_cells {

using U = std::uint32_t;

// Data the builders read and write, all Capcom's addresses (docs/tex-cells.md).
// kFontCache: now Font_TexCache (symbols.toml). Font_TexCache: 128 entries of 0x14
constexpr U kFontEntryBytes = 0x14;
// kCellCache: now D3d_CellTexCache (symbols.toml). D3d_CellTexCache: 128 entries of 0x28
constexpr U kCellEntryBytes = 0x28;
// kCellTable: now SpriteCell_Table (symbols.toml). SpriteCell_Table: 8-byte records
constexpr ::bof3::image::Va kBackdrop{0x6BEA00};     // Dd_CellBackdrop: a 320 x 240 surface, or 0
constexpr ::bof3::image::Va kGlyphData{0x7CC35C};    // Font_GlyphData: 288-byte 4-bit glyphs of 24 x 24
// kVram: now Gfx_VramShadow (symbols.toml). Gfx_VramShadow: 1024 x 512 cells of 16 bits, rows 0x800 bytes
constexpr ::bof3::image::Va kClutRows{0x6C2A40};     // Gfx_ClutRows: u32 generation, u32 pointer, per VRAM row
// kRenderFlags: now Gfx_RenderFlags (symbols.toml). Gfx_RenderFlags: bit 0 the software surfaces, bit 5 colour-keyed textures
constexpr ::bof3::image::Va kStage{0x7CC344};        // Dd_StageSurface: 320 x 256
constexpr ::bof3::image::Va kDevice{0x7CC350};       // D3d_Device, an IDirect3DDevice3 *
// kPixelFormat: now Gfx_PixelFormat (symbols.toml). Gfx_PixelFormat: byte +3 bytes per pixel
// D3d_DeviceDesc 0x7CC238, a D3DDEVICEDESC: the triangle caps' dwTextureCaps
// (+0x84) and the texture size limits (+0xAC..+0xB8).
// kTexCaps: now D3d_DeviceDesc[0x84] (symbols.toml). low byte: bit 1 D3DPTEXTURECAPS_POW2, bit 5 SQUAREONLY
constexpr ::bof3::image::Va kMinTexWidth{0x7CC2E4};
constexpr ::bof3::image::Va kMinTexHeight{0x7CC2E8};
constexpr ::bof3::image::Va kMaxTexWidth{0x7CC2EC};
constexpr ::bof3::image::Va kMaxTexHeight{0x7CC2F0};

using Unpack = void(__cdecl*)(void* dst, const void* src, const void* palette, int w, int h, int pitch);

struct Callees {
    void (__cdecl* unpack_glyph)(void* dst, const void* src, const void* palette, int pitch);   // Font_UnpackGlyph
    Unpack unpack4;        // Cell_Unpack4
    Unpack unpack8;        // Cell_Unpack8
    Unpack unpack4_flip;   // Cell_Unpack4Flip
    Unpack unpack8_flip;   // Cell_Unpack8Flip
    void* (__cdecl* clut_pixels)(unsigned clut);   // Gfx_ClutPixels
};

extern const Callees kOriginals;
extern Callees g;

void SelfTest();   // tex_cells_fuzz.cpp

}  // namespace tex_cells
