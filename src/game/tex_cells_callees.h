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
constexpr U kFontCache = 0x7C9F50;    // Font_TexCache: 128 entries of 0x14
constexpr U kFontEntryBytes = 0x14;
constexpr U kCellCache = 0x7CAE38;    // D3d_CellTexCache: 128 entries of 0x28
constexpr U kCellEntryBytes = 0x28;
constexpr U kCellTable = 0x6BEA18;    // SpriteCell_Table: 8-byte records
constexpr U kBackdrop = 0x6BEA00;     // Dd_CellBackdrop: a 320 x 240 surface, or 0
constexpr U kGlyphData = 0x7CC35C;    // Font_GlyphData: 288-byte 4-bit glyphs of 24 x 24
constexpr U kVram = 0x6C9F44;         // Gfx_VramShadow: 1024 x 512 cells of 16 bits, rows 0x800 bytes
constexpr U kClutRows = 0x6C2A40;     // Gfx_ClutRows: u32 generation, u32 pointer, per VRAM row
constexpr U kRenderFlags = 0x6C3A4C;  // Gfx_RenderFlags: bit 0 the software surfaces, bit 5 colour-keyed textures
constexpr U kStage = 0x7CC344;        // Dd_StageSurface: 320 x 256
constexpr U kDevice = 0x7CC350;       // D3d_Device, an IDirect3DDevice3 *
constexpr U kPixelFormat = 0x7DED60;  // Gfx_PixelFormat: byte +3 bytes per pixel
// D3d_DeviceDesc 0x7CC238, a D3DDEVICEDESC: the triangle caps' dwTextureCaps
// (+0x84) and the texture size limits (+0xAC..+0xB8).
constexpr U kTexCaps = 0x7CC2BC;      // low byte: bit 1 D3DPTEXTURECAPS_POW2, bit 5 SQUAREONLY
constexpr U kMinTexWidth = 0x7CC2E4, kMinTexHeight = 0x7CC2E8, kMaxTexWidth = 0x7CC2EC, kMaxTexHeight = 0x7CC2F0;

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
