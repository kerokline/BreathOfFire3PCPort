// Internal to field_misc.cpp and field_misc_fuzz.cpp: the addresses the group's
// functions touch that have no name in symbols.toml, and every call they make -
// through pointers, so that the start-up fuzz can stand recording functions in
// for them, for the originals' copies and for ours alike. Four callees are
// ours in this module (AreaMap_TooSteepAt under AreaMap_BlockedNarrow,
// Gpu_SetPolyF4 under MapCell_FlatOverlay, MapView_ItemAt under
// AreaMap_ApplyPatch); through the pointers each function is still tested
// alone. docs/field-misc.md.
#pragma once

#include <cstdint>

#include "bof3/symbols.gen.h"

namespace field_misc {

using U = std::uint32_t;

// --- Addresses without a name ------------------------------------------------
// A draw item's two linked faces: the u16 draw-item indices at +0x7E and
// +0x8E of the 0x90-byte item (the second 0x48-byte half's +0x36 and +0x46),
// read by MapCell_FlatOverlay and AreaMap_ApplyPatch for a face 4 / 2 and a
// sub-kind 2+ / 1. Written by code nobody has read.
constexpr U kItemFaceB = 0x905EFE;   // DrawItems + 0x7E
constexpr U kItemFaceA = 0x905F0E;   // DrawItems + 0x8E
// The area header's colour words (header +0x1A), 15-bit BGR, indexed by
// MapCell_FlatOverlay's face bits >> 3.
constexpr U kOverlayColours = 0x8CB59A;
// The dword before a cell run (its count in the high half): AreaMap_Header - 4.
constexpr U kRunHeads = 0x8CB57C;
// The area header's word +2: the offset MapView_CellToMap adds (the texture
// words' base, in words, doubled).
constexpr U kHeaderOffset = 0x8CB582;
// The two D3D constants the draw handlers read (d3d_draw_callees.h has them too).
constexpr U kScaleX = 0x7C9F4C;        // D3d_ScaleX, float
constexpr U kScaleY = 0x7C9F48;        // D3d_ScaleY, float
constexpr U kVertices = 0x7CA958;      // D3d_Vertices, 4 x D3DTLVERTEX
constexpr U kDrawTpage = 0x7DED14;     // Gfx_DrawTpage, the low word read
constexpr U kRhwNumerator = 0x5C4610;  // float 0.1
constexpr U kDevice = 0x7CC350;        // D3d_Device
constexpr U kRetOnly = 0x437CC0;       // a bare ret

// The named data, by the address its symbols.gen.h macro names (the macros are
// typed lvalues or pointers, so bof3::addr's constants are shadowed by them).
namespace at {
inline U Of(const volatile void* p) { return static_cast<U>(reinterpret_cast<std::uintptr_t>(p)); }
inline U AreaMap_BytesAt() { return Of(&AreaMap_Bytes); }
inline U AreaMap_CellBaseAt() { return Of(&AreaMap_CellBase); }
inline U AreaMap_CornersAt() { return Of(&AreaMap_Corners); }
inline U AreaMap_HeaderAt() { return Of(AreaMap_Header); }
inline U AreaMap_PatchBaseAt() { return Of(&AreaMap_PatchBase); }
inline U DrawItemsAt() { return Of(DrawItems); }
inline U Draw_OtSlotAt() { return Of(&Draw_OtSlot); }
inline U Gfx_BufferIndexAt() { return Of(&Gfx_BufferIndex); }
inline U Gfx_ClutStripDirtyAt() { return Of(&Gfx_ClutStripDirty); }
inline U Gfx_ClutStripSourceAt() { return Of(Gfx_ClutStripSource); }
inline U Gfx_ClutStripAt() { return Of(Gfx_ClutStrip); }
inline U Gfx_PacketNextAt() { return Of(&Gfx_PacketNext); }
inline U MapView_CellItemsAt() { return Of(MapView_CellItems); }
inline U MapView_CellsAt() { return Of(MapView_Cells); }
inline U MapView_ColumnAt() { return Of(&MapView_Column); }
inline U MapView_HeightScaleAt() { return Of(&MapView_HeightScale); }
inline U MapView_OriginAt() { return Of(MapView_Origin); }
inline U MapView_RedrawAt() { return Of(&MapView_Redraw); }
inline U MapView_RowAt() { return Of(&MapView_Row); }
}  // namespace at

struct Callees {
    unsigned char (__cdecl* cell_blocked)(short, short);                   // AreaMap_CellBlocked 0x518620 (ours)
    unsigned char (__cdecl* too_steep)(long, long);                        // AreaMap_TooSteep 0x518760 (ours)
    unsigned char (__cdecl* too_steep_at)(long, long);                     // AreaMap_TooSteepAt 0x5187A0 (this module)
    unsigned char* (__cdecl* set_poly_f4)(unsigned char*);                 // Gpu_SetPolyF4 0x5A75B0 (this module)
    void (__cdecl* commit_prim)(unsigned, unsigned);                       // Gfx_CommitPrim 0x461E50 (ours)
    unsigned char (__cdecl* test_condition)(unsigned long);                // Area_TestCondition 0x56FF00 (ours)
    unsigned long (__cdecl* item_at)(long, long);                          // MapView_ItemAt 0x572ED0 (this module)
    void (__cdecl* set_texture)(unsigned long, unsigned char*, int);       // Prim_SetTexture 0x572A00 (ours)
    void (__cdecl* check_height_scale)();                                  // MapView_CheckHeightScale 0x572590 (ours)
    long (__cdecl* slope)(long, long, unsigned long);                      // AreaMap_Slope 0x5722D0 (ours)
    void (__cdecl* prim_color)(unsigned, unsigned, unsigned, unsigned, unsigned, unsigned long*,
                               unsigned long*);                            // D3d_PrimColor 0x59FBA0 (ours)
    void (__cdecl* ret_only)(unsigned);                                    // 0x437CC0, a bare ret
    void (__cdecl* set_blend)(unsigned, unsigned);                         // D3d_SetBlend 0x59FCA0 (ours)
    void (__cdecl* set_shade)(unsigned);                                   // D3d_SetShadeMode 0x59FD80 (ours)
};
extern const Callees kOriginals;
extern Callees g;

// BOF3X_SHADOW=field_misc: the start-up fuzz, field_misc_fuzz.cpp. Clones every
// original before FieldMisc_Inject patches it.
void SelfTest();

}  // namespace field_misc
