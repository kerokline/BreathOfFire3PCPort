// Internal to worldmap_area.cpp and worldmap_area_fuzz.cpp: the addresses the
// world map's area code touches that have no name in symbols.toml, and every
// call it makes - through pointers, so that the start-up fuzz can stand
// recording functions in for them, for the originals' copies and for ours
// alike. docs/worldmap_area.md.
//
// Calls into other groups' functions of the eighth round go through the raw
// addresses below and are never bound here:
//   0x579F00  a map cell's byte set, (x, z, value) - AreaMap_Bytes +
//             AreaMap_Header[0] * z + x (group DD's)
//   0x57C7A0  ScriptFlags_Clear40 (group DD's)
//
// The dispatches that are not calls to a named function read their .data
// table in place, as the originals do, the index unchecked (the fuzz swaps
// the entries for recorders and puts them back):
//   WorldMap33_PlateStates 0x5EF5DC by +1, WorldMapHud_States 0x5EF5F0 by
//   +1, WorldMapHud_BoxStates 0x5EF608 by +3, the world-map records
//   0x653910 by WorldMap_RecordIndex (+0, +0xC, +0x10 of the 0x1C-byte
//   record), EffectKind06_States 0x653EC8 by +1, EffectKind06_Ticks 0x653EDC
//   by +6, EffectKind18_States 0x65406C by +1.
#pragma once

#include <cstdint>

namespace worldmap_area {

namespace at {

// Area 29's init (0x4037B0): eight 1-byte chances summing to 64 and eight
// (x, z) cell pairs, and the eight field objects it keeps one of.
// kArea29Weights: now Area29_Weights (symbols.toml). Area29_Weights, u8 x 8
// kArea29Cells: now Area29_Cells (symbols.toml). Area29_Cells, (u8 x, u8 z) x 8
// kFieldObjects: now Sprite_Objects (symbols.toml). Sprite_Objects: records of 0xA4 bytes
constexpr std::uint32_t kFieldObjectSize = 0xA4;
constexpr std::uint32_t kFieldObjectsEnd = 0x7DF3A0; // the eighth record's end
// The leader's working record (ObjTrio + 0): +9 a step count (u8), +0xC / +0x10
// a direction (dwords), +0x34 / +0x38 the position (16.16), +0x134 a dword the
// area-29 init writes, less 5, to Field_EdgeBits.
// kLeader: now ObjTrio (symbols.toml).
// kLeaderSteps: now ObjTrio[0].step_frames (symbols.toml).
// kLeaderDirX: now ObjTrio[0].step_x (symbols.toml).
// kLeaderDirZ: now ObjTrio[0].step_z (symbols.toml).
// kLeaderX: now ObjTrio[0].x (symbols.toml).
// kLeaderZ: now ObjTrio[0].z (symbols.toml).
// kLeaderEdge: now ObjTrio_addr + 0x134 (symbols.toml).
// The place the party stands on, a u16 (the plate table's and the message
// table's key; written by the world map's frame code, read here).
// kPlace: now AreaW0b_Place (symbols.toml).
// The place-message machine's three bytes (0x9039F4 the state, s8).
// kMsgState: now AreaW0c_TailState (symbols.toml).
// kMsgByteBefore: now AreaW0a_TailKind (symbols.toml).
// kMsgByteAfter: now AreaW0c_TailSub (symbols.toml).
// The world map's mode byte (docs/world-map-hud.md section 6).
// kMapMode: now AreaW0b_MapMode (symbols.toml).
// The leader's cells, the high words of Field_Kind2X / Field_Kind2Z.
constexpr std::uint32_t kLeaderCellX = 0x905E66;
constexpr std::uint32_t kLeaderCellZ = 0x905E62;
// AreaMap_Header's second byte: the map's height in cells (by its use here).
// kMapHeight: now AreaMap_Header[1] (symbols.toml).
// Tables in .data (symbols.toml names them).
// kPlateAnims: now WorldMap33_PlateAnims (symbols.toml). WorldMap33_PlateAnims: (u16 place, u8 animation, u8) x 6, unbounded search
// kPlaceMessages: now WorldMap33_PlaceMessages (symbols.toml). WorldMap33_PlaceMessages: 6 rows of (u16 place, u16 message x 15)
constexpr std::uint32_t kPlaceMessagesEnd = 0x5EF5DC;
// kPlateStates: now WorldMap33_PlateStates (symbols.toml). WorldMap33_PlateStates, 5
// kHudStates: now WorldMapHud_States (symbols.toml). WorldMapHud_States, 2
// kBoxStates: now WorldMapHud_BoxStates (symbols.toml). WorldMapHud_BoxStates, 4
// kDriftUBase: now WorldMap33_DriftUV (symbols.toml). WorldMap33_DriftUV: u8 by +0xB - 2, then v base +4, cell size +8
// kDriftVBase: now WorldMap33_DriftUV[4] (symbols.toml).
// kDriftSize: now WorldMap33_DriftUV[8] (symbols.toml).
// kRecords: now WorldMap_Records (symbols.toml). WorldMap_Records, 0x1C bytes each, the area byte at +0x18
constexpr std::uint32_t kRecordSize = 0x1C;
// kRecordAreas: now WorldMap_Records[6] (symbols.toml). the first record's area byte
constexpr std::uint32_t kRecordAreasEnd = 0x653A5C;  // eleven records
// kKind06States: now EffectKind06_States (symbols.toml). EffectKind06_States, 3
// kKind06Anims: now EffectKind06_Anims (symbols.toml). EffectKind06_Anims, u8 by +6
// kKind06Ticks: now EffectKind06_Ticks (symbols.toml). EffectKind06_Ticks, 6
// kKind18States: now EffectKind18_States (symbols.toml). EffectKind18_States

}  // namespace at

// Callees with no name in symbols.gen.h: other groups' functions this round.
constexpr std::uint32_t kSetCell = bof3::addr::AreaMap_SetByte;      // group DD's
constexpr std::uint32_t kFlagsClear40 = bof3::addr::ScriptFlags_Clear40; // group DD's; ScriptFlags_Clear40

using Handler = void (__cdecl*)();

struct Callees {
    int (__cdecl* rand)();                                          // Rand 0x5B93D2 (the CRT's)
    long (__cdecl* elevation)(long, long);                          // AreaMap_Elevation 0x5720C0 (ours)
    void (__cdecl* set_cell)(int, int, int);                        // 0x579F00 (DD's)
    void (__cdecl* flags_set40)();                                  // ScriptFlags_Set40 0x57C7C0 (ours)
    void (__cdecl* flags_clear40)();                                // 0x57C7A0 (DD's)
    void (__cdecl* open_script)(unsigned short);                    // Msg_OpenScript 0x4976D0 (ours)
    unsigned char (__cdecl* byte_at)(short, short);                 // AreaMap_ByteAt 0x536700 (ours)
    void (__cdecl* set_animation)(unsigned char);                   // Sprite_SetAnimation 0x5891F0 (ours)
    void (__cdecl* pin_sprite)();                                   // WorldMap_PinSprite 0x4112A0 (ours)
    void (__cdecl* queue_overlay)();                                // Sprite_QueueOverlay 0x5890E0 (ours)
    void (__cdecl* effect_release)();                               // Effect_Release 0x589840 (ours)
    void (__cdecl* frame_step)();                                   // WorldMap_FrameStep 0x404160 (ours, world_map)
    void (__cdecl* box_step)();                                     // WorldMapHud_BoxStep 0x404230 (this module)
    void (__cdecl* draw_hud)(int, int);                             // WorldMap_DrawHud 0x404620 (ours, world_map)
    unsigned (__cdecl* record_index)();                             // WorldMap_RecordIndex 0x462A90 (this module)
    unsigned char (__cdecl* set_bank)(unsigned short);              // Sprite_SetAnimationBank 0x589590 (ours)
    unsigned char (__cdecl* script_tick)();                         // Sprite_ScriptTick 0x5893A0 (ours)
    void (__cdecl* update_screen)();                                // Sprite_UpdateScreen 0x588F20 (ours)
    // WorldMap33_DrawDrift's
    void (__cdecl* set_poly_ft4)(unsigned char*);                   // Gpu_SetPolyFT4 0x5A75D0 (ours)
    void (__cdecl* set_shade_tex)(unsigned char*, unsigned);        // Gpu_SetShadeTex 0x5A77A0 (ours)
    void (__cdecl* set_semi_trans)(unsigned char*, unsigned);       // Gpu_SetSemiTrans 0x5A7780 (ours)
    // Gte_RotTransPers4 0x5A85F0 (ours): the original pushes TEN arguments -
    // libgte's (v0..v3, sxy0..sxy3, p, flag) - and ours reads nine (cdecl: the
    // tenth is harmless); handed both locals as the original does.
    long (__cdecl* rot_trans_pers4)(const short*, const short*, const short*, const short*, float*, float*, float*,
                                    float*, long*, long*);
    void (__cdecl* prim_depths)(void*);                             // Gte_PrimDepths4_10 0x5A9290 (ours)
    void (__cdecl* set_texture)(unsigned long, unsigned char*, int);   // Prim_SetTexture 0x572A00 (ours)
    void (__cdecl* commit_prim)(unsigned, unsigned);                // Gfx_CommitPrim 0x461E50 (ours)
    unsigned char* (__cdecl* item_half_at)(long, long);             // MapView_ItemHalfAt 0x572F70 (ours)
    void (__cdecl* link_prim_at)(unsigned long, unsigned long, int, unsigned);   // MapView_LinkPrimAt 0x572FA0 (ours)
};

extern const Callees kOriginals;
extern Callees g;

// BOF3X_SHADOW=worldmap_area: the start-up fuzz, worldmap_area_fuzz.cpp.
// Clones every original before WorldmapArea_Inject patches it.
void SelfTest();

}  // namespace worldmap_area
