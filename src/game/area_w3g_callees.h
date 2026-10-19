// Internal to area_w3g.cpp and area_w3g_fuzz.cpp: the cells and tables world
// 3's areas 148..151 touch that symbols.toml has no name for (the tables it
// names are listed too, by address, for the fuzz's regions), area 151's
// world-map table set, and the callees nobody owns, by raw address.
// docs/area_w3g.md.
//
// Raw-address callees (the round's rebinding pass names them):
//   0x494060  (): sets the map camera up for a draw (area_w3f_callees.h's
//             kSetMapCamera). Engine; nobody's.
//   0x494110  (const long* point, float* vertex): projects a world point into
//             a vertex of three dwords (x, y floats and a depth) at its second
//             argument (area_w3f_callees.h's kProjectPoint). Engine; nobody's.
//   0x4941E0  (const long* point, const short* offset, short* out): the
//             point through the camera (0x5A8200), then out[0] / out[1] =
//             offset[0] / offset[1] * 1000 / its depth (32-bit idiv): a
//             screen-space size at the point. Engine; nobody's (read
//             2026-09-28 for this group).
//   0x56FCA0  (): the view shift after a focus change (field_modes_callees.h's
//             kViewShift, PSX 0x80155154). Engine; nobody's.
#pragma once

#include <cstdint>

#include "bof3/symbols.gen.h"

// Rebound 2026-09-28 (round ten's cleanup, docs/round-10-cleanup.md item 1):
// every constant here whose target is ours reads bof3::addr::<Name>. The values
// are unchanged - the fuzz keys on them - and the comments' "nobody owns" is as
// of the wave that wrote them.
namespace area_w3g {
namespace at {

using U = std::uint32_t;

// --- the field frame's cells ------------------------------------------------
// kStoryFlags: now Cond_Flags[0xa0] (symbols.toml). the flag bank most Flags_* calls here are handed
// kFlags904000: now Cond_Flags[0x70] (symbols.toml). the bank areas 149 and 150 test (flags 0x11, 3)
constexpr U kTailKind = 0x9039F3;         // Field_ModeTailKinds' index (s8)
constexpr U kTailState = 0x9039F4;        // s8: the armed tail kind's (or the place hook's) state
constexpr U kTailArg = 0x9039F5;          // the tail's byte: area 148 keeps an effect slot in it
constexpr U kTailTimer = 0x9039F6;        // u16: the tails' frame counter
// kLeader: now ObjTrio (symbols.toml). ObjTrio + 0, the leader's record
// kLeaderDir: now ObjTrio[8] (symbols.toml). the leader's +8, its facing
// kLeaderX: now ObjTrio[0x34] (symbols.toml). the leader's +0x34 / +0x38, 16.16
// kLeaderZ: now ObjTrio[0x38] (symbols.toml).
constexpr U kPartyStride = 0x14C;
constexpr U kMemberId = 0x802DC9;         // party record 0's +0x89: the member's id
// kCursor: now AreaW0c_ChoiceAnswer (symbols.toml). the message box's cursor (s8): a choice's answer
constexpr U kMessage = 0x7DEE48;          // u16: the message a choice opened, 0xFFFF none
constexpr U kCounter0 = 0x903848;         // MoveScript counter 0 (flow ops A0..AC)
constexpr U kScriptVar = 0x90384B;        // the movement script's byte the tails wait on
// kCameraAngle1: now Camera_Angles[1] (symbols.toml). s16 Camera_Angles[1]
// kCameFrom: now AreaW2c_LastArea (symbols.toml). u16: the area the party came from (docs/mode-flow.md)
// kEffects: now Effect_Objects (symbols.toml). Effect_Objects: records of 0x80
constexpr U kEffectStride = 0x80;
constexpr unsigned kEffectCount = 20;
// kExtra0: now Sprite_ObjectsExtra (symbols.toml). Sprite_ObjectsExtra record 0
// kKind2X: now Sprite_Kind2[0x34] (symbols.toml). Sprite_Kind2 + 0x34 (s32)
// kKind2Height: now Sprite_Kind2[0x3e] (symbols.toml). Sprite_Kind2 + 0x3E (u16)
// kFocusX: now MapView_FocusX (symbols.toml). MapView_FocusX (dword)
// kOrigin: now MapView_Origin (symbols.toml). MapView_Origin (its first word)
// kDirSteps: now Field_DirectionSteps (symbols.toml). Field_DirectionSteps: (x, z) s32 a direction
// kWalkDelta: now MoveCmd_F9Steps (symbols.toml). s32 x, z per direction, 8 bytes apart (event_ops_callees.h's)
// kHealByte: now AreaW3g_HealByte (symbols.toml). area 150's tail stores (docs/area_w3g.md section 4)
// kWord90405C: now Cond_Flags[0xcc] (symbols.toml).
// kByte90405E: now Cond_Flags[0xce] (symbols.toml).
// kByte90405F: now Cond_Flags[0xcf] (symbols.toml).
// kByte929EC1: now AreaW3g_Byte929EC1 (symbols.toml).
// kByte9036D0: now AreaW3g_Byte9036D0 (symbols.toml).

// --- area 148 (descriptor 0x635388) -----------------------------------------
// kA148BeamColours: now Area148_BeamColours (symbols.toml). (r, g, b) by the effect's +6
// kA148MemberIdsA: now Area148_MemberIdsA (symbols.toml). 4 member ids, then 4 u16 messages at 0x6353DC
// kA148MessagesA: now Area148_MessagesA (symbols.toml).
// kA148MemberIdsB: now Area148_MemberIdsB (symbols.toml). the same for handler 5
// kA148MessagesB: now Area148_MessagesB (symbols.toml).
constexpr U kA148Switch = 0x6353F0;       // one (x, z, facing, tail state) record: the cell hook's
constexpr U kA148SwitchEnd = 0x6353F4;
// kA148BeamStates: now Area148_BeamStates (symbols.toml). effect kind 0x7E's three states
constexpr unsigned kA148BeamStateCount = 3;
// kA148TurnSteps: now Area148_TurnSteps (symbols.toml). 10 pairs of s8 indexes by +9 >> 3
constexpr U kA148TurnCells = 0x635414;    // (dx, dz) s8 pairs

// --- area 149 (descriptor 0x6361B8) -----------------------------------------
// kA149DustStates: now Area149_DustStates (symbols.toml). handler 1's four states by Sprite_Current[4]
constexpr unsigned kA149DustStateCount = 4;

// --- area 150 (descriptor 0x636A18) -----------------------------------------
// kA150MessagesA: now Area150_MessagesA (symbols.toml). u16 by the s8 cursor (choice 0), unchecked
// kA150MessagesB: now Area150_MessagesB (symbols.toml). u16 by the s8 cursor (choice 1), unchecked

// --- the world maps' cells (area 16's, docs/area_w0b.md; as area_w3a's) --------
// kLeaderSteps: now ObjTrio[9] (symbols.toml). u8 step count
// kLeaderDirX: now ObjTrio[0xc] (symbols.toml). dword direction
// kLeaderDirZ: now ObjTrio[0x10] (symbols.toml).
// kLeaderCellWordX: now ObjTrio[0x36] (symbols.toml). the high word of +0x34
// kLeaderCellWordZ: now ObjTrio[0x3a] (symbols.toml). the high word of +0x38
// kPlace: now AreaW0b_Place (symbols.toml). u16: the place the party stands on (world map)
// kMapMode: now AreaW0b_MapMode (symbols.toml). the world map's mode byte
constexpr U kLeaderCellX = 0x905E66;      // the high words of Field_Kind2X / Z
constexpr U kLeaderCellZ = 0x905E62;
// kMapHeight: now AreaMap_Header[1] (symbols.toml). AreaMap_Header's second byte
// kButtonMap0: now Gfx_UnpackScratch[0x1f000] (symbols.toml). the field's button map: word 0, the first legend's button
// kButtonMap6: now Gfx_UnpackScratch[0x1f00c] (symbols.toml). word 6, the second's
// kPartySet: now AreaW0b_PartySet (symbols.toml). & 0x7F: the party set (0xC shows the third legend)
constexpr U kAreaText = 0x803580;         // the area's text section; area 151 reads its label offset from its first dword
constexpr U kItemsHeld = 0x9040EC;        // a byte per item id: 0 not yet seen
// kItemNameKey: now AreaW0b_ItemNameKey (symbols.toml). the 16-byte name the hook copies for item 0x16
constexpr U kTextRecords = 0x904CE0;      // Text_Records: the place hook fills rows of 0x20 from here
constexpr U kFlag3A79 = 0x903A79;         // the record +4 state: 9 releases the effect
// kMsgMode: now AreaW0a_TailKind (symbols.toml).
// kMsgState: now AreaW0c_TailState (symbols.toml).
// kMsgArg: now AreaW0c_TailSub (symbols.toml).

// --- a world-map copy's tables (area 45's layout, docs/area_w1b.md section 5;
// area_w3a_callees.h's struct less what area 151 has not: no cell search, no
// name-set search, no plate bank of its own) ------------------------------------
struct WorldMapTables {
    const char* name;                 // "Area151": the messages' prefix
    U plate_anims;                    // (u16 place, u8 animation, u8), searched with NO bound
    U cells;                          // (u8 x, u8 z, u8, u8 id): Record4MarkCell's, by +0xB unchecked
    U place_messages, place_messages_end;   // rows of 0x20: u16 place, fifteen u16 messages
    U name_items;                     // the one name set's items (its id byte is the one before)
    U text_rows_end;                  // the place hook's last Text_Records row (exclusive)
    U plate_states, hud_states, frame_states, box_states;
    U sprites, buttons;               // (w, h, u, v) x 22; (u16 mask, u8 sprite, u8) x 6, the second legend reads eight
    U record8_states, directions, record8_anims, record4_states;
    U drift_u, drift_v, drift_size;   // u8 by +0xB - 2 each
    U label_offset;                   // the dword whose low word is the region label's offset in kAreaText
    // the copy's own functions another of them calls directly
    U fn_frame_step, fn_frame_hold, fn_box_step, fn_draw_frame, fn_draw_sprite, fn_draw_hud;
};

// Area 151: WorldMap_Records record 9, WorldMap_FieldHooks entry 9; its
// descriptor 0x637078 (only a choice table, +0x34 -> 0x637070, whose one entry
// is Area130_ChoiceTailState2, and no init). Its plate state 0 is 0x424BA0,
// the plate start with bank 0x1D1 that area 152's table names too - the
// linker's fold, in group AR4A's band.
constexpr WorldMapTables kWm151 = {
    "Area151",
    0x636A68,             // 7 plate animations (then the cell record)
    0x636A84,             // 1 cell record (then the descriptor's +0x20 data)
    0x6370BC, 0x63719C,   // 7 place rows
    0x63719D, 0x904D80,   // the name set (id 0x10) at 0x63719C: five items; five text rows
    0x6371A4, 0x6371B8, 0x6371C0, 0x6371D0,
    0x6371E0, 0x637238,
    0x637250, 0x63725C, 0x63726C, 0x637274,
    0x63727C, 0x637280, 0x637284,
    0x803580,
    bof3::addr::Area151_FrameStep, bof3::addr::Area151_FrameHold, bof3::addr::Area151_BoxStep,
    bof3::addr::Area151_DrawFrame, bof3::addr::Area151_DrawSprite, bof3::addr::Area151_DrawHud,
};

}  // namespace at

// The unowned callees (above).
constexpr std::uint32_t kSetMapCamera = bof3::addr::EffectGte_LoadMapCamera;
constexpr std::uint32_t kProjectPoint = bof3::addr::EffectGte_ProjectPoint;
constexpr std::uint32_t kScreenSize = bof3::addr::EffectGte_ProjectSize;
constexpr std::uint32_t kViewShift = bof3::addr::MapView_FillCells;

}  // namespace area_w3g
