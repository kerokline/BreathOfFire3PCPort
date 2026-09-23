// Internal to tex_page.cpp and tex_page_fuzz.cpp: the calls the page-texture
// builders make that the start-up fuzz stands recorders in for, through
// pointers - the three pixel converters and Gfx_ClutPixels. The surface
// helpers are called by name, and the COM calls go through Dd_DirectDraw and
// the surfaces, which the fuzz replaces with fakes (ddraw_fuzz.h).
// docs/tex-page.md.
#pragma once

#include <cstdint>

namespace tex_page {

using U = std::uint32_t;

// Data the builders read and write, all Capcom's addresses (docs/tex-page.md).
constexpr U kPageCache = 0x6C3F40;    // Gfx_TexCache: 32 pages x 32 entries of 0x18
constexpr U kEntryBytes = 0x18;
constexpr U kVram = 0x6C9F44;         // Gfx_VramShadow: 1024 x 512 cells of 16 bits, rows 0x800 bytes
constexpr U kClutRows = 0x6C2A40;     // Gfx_ClutRows: u32 generation, u32 pointer, per VRAM row
constexpr U kTexKey = 0x7DED0C;       // Gfx_TexCacheKey: s16 x, y, w, h
constexpr U kRenderFlags = 0x6C3A4C;  // Gfx_RenderFlags: bit 0 the software surfaces, bit 5 colour-keyed textures
constexpr U kDirectDraw = 0x7CC334;   // Dd_DirectDraw, an IDirectDraw4 *
constexpr U kStage = 0x7CC344;        // Dd_StageSurface: 320 x 256, made by 0x5A5160 through Dd_CreatePlainSurface
constexpr U kPixelFormat = 0x7DED60;  // Gfx_PixelFormat: byte +3 bytes per pixel, +4/+8/+0xC shifts, +0x10/+0x14/+0x18 masks
constexpr U kFormats = 0x7DED80;      // the DDPIXELFORMAT at +0x20 of each 0x40-byte format record from 0x7DED60
constexpr U kTexture2Iid = 0x5C4488;  // IID_IDirect3DTexture2 in .rdata

struct Callees {
    void (__cdecl* convert4)(void* dst, const void* src, const void* clut, int w, int h, int pitch);   // Tex_Convert4
    void (__cdecl* convert8)(void* dst, const void* src, const void* clut, int w, int h, int pitch);   // Tex_Convert8
    void (__cdecl* convert16)(void* dst, const void* src, int w, int h, int pitch);                    // Tex_Convert16
    void* (__cdecl* clut_pixels)(unsigned clut);                                                       // Gfx_ClutPixels
};

extern const Callees kOriginals;
extern Callees g;

void SelfTest();   // tex_page_fuzz.cpp

}  // namespace tex_page
