// Internal to map_field_objects.cpp and map_field_objects_fuzz.cpp: the
// addresses group DD's functions touch that have no name in symbols.toml, and
// every call they make - through pointers, so that the start-up fuzz can
// stand recording functions in for them, for the originals' copies and for
// ours alike. One callee is another round-eight group's and is called by its
// raw address (0x516E70, group DB); one is this group's own (AreaMap_SetByte,
// which EventOp_8x calls - through the pointer, so each is tested alone).
// docs/map_field_objects.md.
#pragma once

#include <cstdint>

#include "bof3/symbols.gen.h"

namespace map_field_objects {

using U = std::uint32_t;

// --- Addresses without a name ------------------------------------------------------
// SharedScratch's first three dwords, the PSX scratchpad's 0x1F800000..: the
// uprights' cell origin (x, y) and their base height; EventOp_8x's object
// index (word +0) and animation bank (word +2).
inline auto& kScratchX = SharedScratch;
inline auto& kScratchY = SharedScratch.d_04;
inline auto& kScratchH = SharedScratch.d_08;
constexpr U kScratchBank = 0x903852;
// MapView_ScreenXY's three points: the uprights' Gte_RotTransPers3 outputs.
constexpr U kScreen0 = 0x903820, kScreen1 = 0x903828, kScreen2 = 0x903830;
// Prim_VertexScratch's four vertices, 8 bytes apart.
// kVertex0: now Prim_VertexScratch (symbols.toml).
// kVertex1: now Prim_VertexScratch[4] (symbols.toml).
// kVertex2: now Prim_VertexScratch[8] (symbols.toml).
// kVertex3: now Prim_VertexScratch[0xc] (symbols.toml).
// The area block (AreaMap_Header): width byte +0, height byte +1, the tile
// offset word +2, the corner dwords from +0x30 (AreaMap_Corners), the two
// flat colours +0x1A / +0x1C (MapCell_FlatOverlay's), AreaMap_PatchBase +0x28.
// kHeader: now AreaMap_Header (symbols.toml).
// kHeaderOffset: now AreaMap_Header[2] (symbols.toml).
// kCorners: now AreaMap_Corners (symbols.toml).
constexpr U kFlatColour0 = 0x8CB59A, kFlatColour1 = 0x8CB59C;
// kPatchBase: now AreaMap_PatchBase (symbols.toml).
// The view's cell ring: MapView_CellItems' words +2 (the draw item, low 12 bits).
// kCellItemWords: now MapView_CellItems[2] (symbols.toml).
// The window colour (Config), a CLUT row: s8 (battle_windows_callees.h kColour).
// kWindowColour: now Config_WindowColour (symbols.toml).
// CharacterRecords, 0xA4 bytes each (battle_window_draw_callees.h).
// kCharRecords: now CharacterRecords (symbols.toml).
constexpr U kCharStride = 0xA4;
// The play clock: hours, minutes, seconds, frames (0x9040C8..0x9040CB).
// kClockMinutes: now MapFieldObjects_ClockMinutes (symbols.toml).
// kClockFrames: now MapFieldObjects_ClockFrames (symbols.toml).
constexpr U kClockHours = 0x9040C8;
// The sprintf buffer every number goes through (battle_draw_callees.h kPrintBuf).
constexpr U kPrintBuf = 0x904BA0;
// The formats and texts (.rdata / .data, read 2026-09-25): "%3d", "%3d/   ",
// "   /%3d", "%02d"; two four-byte status words (Shift-JIS), "."; and the
// piece lists Menu_DrawPieces draws (3-byte records to an id of 0xFF).
constexpr U kFormat3d = 0x64E324, kFormatLeft = 0x6639B0, kFormatRight = 0x6639A8, kFormat02d = 0x65306C;
constexpr U kStatusText0 = 0x66A0E8, kStatusText1 = 0x66A0F0, kColonText = 0x6637E0;
// kMemberPieces: now Menu_MemberPieces (symbols.toml).
// kClockPieces: now Menu_PlayTimePieces[0x18] (symbols.toml).
// kClockMiddle: now Menu_PlayTimePieces (symbols.toml).
// kClockLeft: now Menu_PlayTimePieces[0xc] (symbols.toml).
// Sprite_Objects' records, 0xA4 bytes each; EventOp_8x's fields by the
// object index (0x7DEE80 + 0xA4 n + 0x84.. as absolute bases, as the original
// addresses them).
// kObjects: now Sprite_Objects (symbols.toml).
constexpr U kObjectStride = 0xA4;
// kObjFlag84: now Sprite_Objects[0x84] (symbols.toml).
// kObjWord88: now Sprite_Objects[0x88] (symbols.toml).
// kObjX8C: now Sprite_Objects[0x8c] (symbols.toml).
// kObjZ90: now Sprite_Objects[0x90] (symbols.toml).
// kObj94: now Sprite_Objects[0x94] (symbols.toml).
// kObjByteA0: now Sprite_Objects[0xa0] (symbols.toml).
// Field_ActiveMember and Sprite_Current, the two pointers EventOp_8x sets.
// kActiveMember: now Field_ActiveMember (symbols.toml).
// kSpriteCurrent: now Sprite_Current (symbols.toml).
// The 8 px UI font (x, y, colour, count, text) -> text end - group DB's
// (round eight), called through its address (ours, named by symbol).
constexpr U kTinyFont = bof3::addr::Text_DrawSmall;

// The named tables, by the address their symbols.gen.h macros name.
namespace at {
inline U Of(const volatile void* p) { return static_cast<U>(reinterpret_cast<std::uintptr_t>(p)); }
inline U UprightCountsAt() { return Of(MapCell_UprightCounts); }       // 0x66313C
inline U UprightOffsetsXAt() { return Of(MapCell_UprightOffsetsX); }   // 0x663148
inline U UprightOffsetsYAt() { return Of(MapCell_UprightOffsetsY); }   // 0x6631B4
inline U UprightHeightsAt() { return Of(MapCell_UprightHeights); }     // 0x663220
inline U UprightV01At() { return Of(MapCell_UprightV01); }            // 0x663234
inline U UprightV23At() { return Of(MapCell_UprightV23); }       // 0x663248
inline U UprightU02At() { return Of(MapCell_UprightU02); }          // 0x66325C
inline U UprightU13At() { return Of(MapCell_UprightU13); }         // 0x663268
inline U UprightClutAt() { return Of(MapCell_UprightClut); }           // 0x663274
inline U OpLengthsAt() { return Of(EventScript_OpLengths); }           // 0x663B0C
inline U OtSlotAt() { return Of(&Draw_OtSlot); }                       // 0x92BF19
inline U CondByteFFAt() { return Of(&Cond_ByteFF); }                   // 0x7E1BE2
inline U FrameCounterAt() { return Of(&Frame_Counter); }               // 0x937F94
inline U PacketNextAt() { return Of(&Gfx_PacketNext); }                // 0x7E0670
inline U OriginAt() { return Of(MapView_Origin); }                     // 0x7E0688
inline U RowAt() { return Of(&MapView_Row); }                          // 0x929F24
inline U ColumnAt() { return Of(&MapView_Column); }                    // 0x929F20
inline U BytesAt() { return Of(&AreaMap_Bytes); }                      // 0x905D94, the pointer
inline U StatusBitsAt() { return Of(&Field_StatusBits); }              // 0x8034E1
inline U ScriptFlagsAt() { return Of(&Field_ScriptFlags); }            // 0x9039A2
}  // namespace at

// Every callee, typed with 32-bit arguments (the originals push whole
// registers; each callee's reading is noted where it matters) and, where one
// of ours hands a callee's eax on, returning it whole.
struct Callees {
    // the map-cell handlers
    long (__cdecl* elevation)(long, long);                                     // AreaMap_Elevation 0x5720C0
    long (__cdecl* rtp3)(const short*, const short*, const short*, float*, float*, float*, long*);  // Gte_RotTransPers3 0x5A84A0
    void (__cdecl* depth_f3)(float*, float*, float*);                          // Gte_StoreDepthF3 0x5A9130
    void (__cdecl* set_poly_ft4)(unsigned char*);                              // Gpu_SetPolyFT4 0x5A75D0
    void (__cdecl* set_shade_tex)(unsigned char*, unsigned);                   // Gpu_SetShadeTex 0x5A77A0
    U (__cdecl* commit)(unsigned, unsigned);                                   // Gfx_CommitPrim 0x461E50
    long (__cdecl* rtp4)(const short*, const short*, const short*, const short*, float*, float*, float*, float*,
                         long*);                                               // Gte_RotTransPers4 0x5A85F0
    void (__cdecl* depth_f4)(float*, float*, float*, float*);                  // Gte_StoreDepthF4 0x5A9170
    void (__cdecl* depths4)(void*);                                            // Gte_PrimDepths4_10 0x5A9290
    void (__cdecl* set_texture)(unsigned long, unsigned char*, int);           // Prim_SetTexture 0x572A00
    void (__cdecl* flat_overlay)(int, unsigned);                               // MapCell_FlatOverlay 0x570AB0
    void (__cdecl* apply_patch)(const unsigned char*);                         // AreaMap_ApplyPatch 0x571110
    unsigned char (__cdecl* test)(unsigned long);                              // Area_TestCondition 0x56FF00
    long (__cdecl* rtp)(const short*, unsigned long*, long*);                  // Gte_RotTransPers 0x5A8250
    void (__cdecl* depth_flat4)(void*);                                        // Gte_PrimDepthFlat4_10 0x5A92E0
    // the area set-up entries
    long (__cdecl* clut_adjust)(int, int, int, int, int);                      // Gfx_ClutAdjust 0x5718F0
    // the menu draws
    void (__cdecl* draw_box)(int, int, int, int, int, int);                    // Menu_DrawBox 0x57CF60
    void (__cdecl* item_icon)(int, int, int, int);                             // Menu_DrawItemIcon 0x573F30
    const unsigned char* (__cdecl* text_at)(int, int, int, int, const unsigned char*);  // Text_DrawAt 0x516B30
    int (__cdecl* sprintf)(char*, const char*, unsigned);                      // Crt_sprintf 0x5B9380 (Capcom's CRT)
    void (__cdecl* font8)(int, int, int, const unsigned char*);                // Text_DrawFont8 0x517090
    const unsigned char* (__cdecl* tiny_font)(int, int, unsigned, unsigned, const unsigned char*);  // 0x516E70 (DB)
    U (__cdecl* pieces)(int, int, const unsigned char*, int);                  // Menu_DrawPieces 0x57D910
    U (__cdecl* exp_bar)(int, int, unsigned, unsigned, unsigned);              // Menu_DrawExpBar 0x574530
    void (__cdecl* set_sprt8)(unsigned char*);                                 // Gpu_SetSprt8 0x5A7720
    void (__cdecl* set_semi)(unsigned char*, unsigned);                        // Gpu_SetSemiTrans 0x5A7780
    void (__cdecl* font12)(int, int, int, const unsigned char*);               // Text_DrawFont12 0x516F60
    // the event script
    const unsigned char* (__cdecl* skip_control)(const unsigned char*);        // EventScript_SkipControl 0x579AA0
    void (__cdecl* obj_reset)();                                               // EventObj_Reset 0x579E30
    U (__cdecl* set_bank)(U);                                                  // Sprite_SetAnimationBank 0x589590 (a word)
    void (__cdecl* set_flags)(const unsigned char*);                           // EventObj_SetFlags 0x579DB0
    void (__cdecl* set_animation)(U);                                          // Sprite_SetAnimation 0x5891F0 (a byte)
    U (__cdecl* set_byte)(U, U, U);                                            // AreaMap_SetByte 0x579F00 (this group's)
};
extern const Callees kOriginals;
extern Callees g;

// BOF3X_SHADOW=map_field_objects: the start-up fuzz, map_field_objects_fuzz.cpp.
// Clones every original before MapFieldObjects_Inject patches it.
void SelfTest();

}  // namespace map_field_objects
