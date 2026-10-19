// Internal to d3d_list.cpp and d3d_draw_fuzz.cpp: every call the draw
// Gfx_DrawOTag 0x59EE50 makes, through pointers, so that the start-up fuzz can
// stand recording functions in for them. docs/d3d-draw.md section 1.
#pragma once

#include <cstdint>

#include "bof3/symbols.gen.h"
// Rebound 2026-09-28 (round eleven's cleanup, docs/round-11-cleanup.md item 2):
// every constant here whose target has a name in symbols.toml reads
// bof3::addr::<Name>. The values are unchanged - the fuzz keys on them.

namespace d3d_list {

using U = std::uint32_t;

constexpr U kRetOnly = bof3::addr::BareRet;
// kDevice: now D3d_Device (symbols.toml). D3d_Device
// kRenderFlags: now Gfx_RenderFlags (symbols.toml). Gfx_RenderFlags, byte: bit 0 the software surfaces
// kDrawEnable: now Gfx_DrawEnable (symbols.toml). Gfx_DrawEnable, byte (0x7DED16 the byte before it)
// kDrawTpage: now Gfx_DrawTpage (symbols.toml). Gfx_DrawTpage, u16
// kTexKey: now Gfx_TexCacheKey (symbols.toml). Gfx_TexCacheKey, 8 bytes
// kAfterDrawFlag: now D3d_AfterDrawRequest (symbols.toml). u16: D3d_AfterDraw after this draw when set
constexpr U kSoftBytes = 0x59F304;     // the first table's byte index table (0xD1 bytes)
constexpr U kD3dBytes = 0x59F440;      // the second table's (0xD5 bytes)
constexpr U kFontInUse = 0x7C9F60, kFontInUseEnd = 0x7CA960;   // Font_TexCache +0x10, 128 x 0x14
constexpr U kCellInUse = 0x7CAE4C, kCellInUseEnd = 0x7CC24C;   // D3d_CellTexCache +0x14, 128 x 0x28
constexpr U kHandlers = 21;            // entries 0..20 of each table call a handler

struct Callees {
    long (__cdecl* soft[kHandlers])(const unsigned char* prim);   // first table's entries 0..20
    long (__cdecl* d3d[kHandlers])(const unsigned char* prim);    // second table's
    void (__cdecl* move_image)(short* rect, int to_x, int to_y);  // Gfx_MoveImage
    void (__cdecl* alpha_modulate)(unsigned on);                  // D3d_SetAlphaModulate
    void (__cdecl* after_draw)();                                 // D3d_AfterDraw (a tail jump)
};

extern const Callees kOriginals;
extern Callees g;

void SelfTest();   // d3d_list_fuzz.cpp
// The two byte index tables as d3d_list.cpp holds them (Entry, a switch),
// registered against kSoftBytes / kD3dBytes for the hosted check
// (docs/operand-store.md). D3dDraw_Inject calls it.
void Operands();

}  // namespace d3d_list
