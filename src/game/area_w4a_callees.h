// Internal to area_w4a.cpp and area_w4a_fuzz.cpp: the cells and tables world
// 4's areas 152..155, 166 and 167 touch that symbols.toml has no name for (the
// tables it names are listed too, by address, for the fuzz's regions) and area
// 152's world-map table set. docs/area_w4a.md.
//
// No raw-address callee: every function this band calls is named and ours
// (Flags_*, ScriptFlags_*, Party_DropIn, Field_ChangeArea, Item_NamePtr,
// Inventory_Add, the world map's WorldMap_* / Gpu_* / Gte_* set, ...) or
// Capcom's and in the harness's standard set (Rand).
#pragma once

#include <cstdint>

#include "bof3/symbols.gen.h"

// Rebound 2026-09-28 (round ten's cleanup, docs/round-10-cleanup.md item 1):
// every constant here whose target is ours reads bof3::addr::<Name>. The values
// are unchanged - the fuzz keys on them - and the comments' "nobody owns" is as
// of the wave that wrote them.
namespace area_w4a {
namespace at {

using U = std::uint32_t;

// --- the field frame's cells --------------------------------------------------
// kStoryFlags: now Cond_Flags[0xa0] (symbols.toml). the flag bank every Flags_* call here is handed
// kFlagBankF8: now Cond_Flags[0x68] (symbols.toml). Cond_Flags + 0x68: the bank area 155's choice sets bit 1 of
constexpr U kTailKind = 0x9039F3;         // Field_ModeTailKinds' index (s8)
constexpr U kTailState = 0x9039F4;        // s8: the armed tail kind's (or the field hook's) state
constexpr U kTailArg = 0x9039F5;
constexpr U kTailTimer = 0x9039F6;        // u16: tail kind 32's countdown
// kMsgMode: now AreaW0a_TailKind (symbols.toml).
// kMsgState: now AreaW0c_TailState (symbols.toml).
// kMsgArg: now AreaW0c_TailSub (symbols.toml).
constexpr U kMessage = 0x7DEE48;          // the message word a choice handler leaves (0xFFFF: none)
constexpr U kChoice = 0x7DEE67;           // the choice box's answer (s8)
// kFocusObject: now AreaW1a_FocusObject (symbols.toml). the record whose talk opened the box (docs/area_w2c.md)
constexpr U kScriptVar5 = 0x90384A;       // MoveScript variables 5 and 6 (docs/area_w0a.md)
constexpr U kScriptVar6 = 0x90384B;
// kCameraDistance: now Camera_Distance (symbols.toml). Camera_Distance (u16)
// kByte905E68: now AreaW1a_EntryZone (symbols.toml). a byte area 167's handler 5 tests (the 0x905E60 row)
constexpr U kTextRecords = 0x904CE0;      // Text_Records: area 155's choice copies an item name to row 0
// kEffects: now Effect_Objects (symbols.toml). Effect_Objects: twenty records of 0x80
constexpr U kEffectStride = 0x80;
constexpr unsigned kEffectCount = 20;
constexpr U kLeader = 0x802D40;           // ObjTrio + 0, the leader's record
// kLeaderDir: now ObjTrio[8] (symbols.toml). the leader's +8, its facing

// --- the world maps' cells (area 16's, docs/area_w0b.md) ----------------------
// kLeaderSteps: now ObjTrio[9] (symbols.toml). u8 step count
// kLeaderDirX: now ObjTrio[0xc] (symbols.toml). dword direction
// kLeaderDirZ: now ObjTrio[0x10] (symbols.toml).
// kLeaderX: now ObjTrio[0x34] (symbols.toml). the leader's +0x34 / +0x38, 16.16
// kLeaderZ: now ObjTrio[0x38] (symbols.toml).
// kPlace: now AreaW0b_Place (symbols.toml). u16: the place the party stands on (world map)
// kMapMode: now AreaW0b_MapMode (symbols.toml). the world map's mode byte
constexpr U kLeaderCellX = 0x905E66;      // the high words of Field_Kind2X / Z
constexpr U kLeaderCellZ = 0x905E62;
// kMapHeight: now AreaMap_Header[1] (symbols.toml). AreaMap_Header's second byte
// kButtonMap0: now Gfx_UnpackScratch[0x1f000] (symbols.toml). the field's button map: word 0, the first legend's button
// kButtonMap6: now Gfx_UnpackScratch[0x1f00c] (symbols.toml). word 6, the second's
// kPartySet: now AreaW0b_PartySet (symbols.toml). & 0x7F: the party set (0xC shows the third legend)
constexpr U kAreaText = 0x803580;         // the area's text section; area 152 reads its label offset from its first dword
constexpr U kFlag3A79 = 0x903A79;         // the record +4 state: 9 releases the effect

// --- area 152's world-map tables (area 87's layout, docs/area_w2b.md section 4;
// its field hook is its own: place rows and a chapter, no cell, no name set) ---
struct WorldMapTables {
    const char* name;                 // "Area152": the messages' prefix
    U place_rows, place_rows_end;     // rows of 0x20: u16 place, fifteen u16 messages; searched in bounds
    U plate_anims;                    // (u16 place, u8 animation, u8), searched with NO bound
    U cells;                          // (u8 x, u8 z, u8, u8): Record4MarkCell's by +0xB, unchecked
    U plate_states, hud_states, frame_states, box_states;
    U sprites, buttons;               // (w, h, u, v) x 22; (u16 mask, u8 sprite, u8) x 6, the second legend reads eight
    U record8_states, directions, record8_anims, record4_states;
    U drift_u, drift_v, drift_size;   // u8 by +0xB - 2 each
    U label_offset;                   // the dword whose low word is the region label's offset in kAreaText
    unsigned plate_bank;              // PlateStart's Sprite_SetAnimationBank
    // the copy's own functions another of them calls directly
    U fn_frame_step, fn_frame_hold, fn_box_step, fn_draw_frame, fn_draw_sprite, fn_draw_hud;
};

// Area 152: WorldMap_Records record 10 (0x653A28, area byte 0x98),
// WorldMap_FieldHooks entry 10 (0x662E18); its descriptor 0x637300 (only a
// choice table, +0x34 -> Area130_ChoiceTailState2, and the init 0x4249D0).
// The plate animations and the cell records sit before the descriptor, the
// rest after it.
constexpr WorldMapTables kWm152 = {
    "Area152",
    0x637344, 0x6373A4,   // 3 place rows (then the plate state table)
    0x637288,             // 3 plate animations
    0x637294,             // 2 cell records (then the descriptor's +0x20 data)
    0x6373A4, 0x6373B8, 0x6373C0, 0x6373D0,
    0x6373E0, 0x637438,
    0x637450, 0x63745C, 0x63746C, 0x637474,
    0x63747C, 0x637480, 0x637484,
    0x803580, 0x1D1,
    bof3::addr::Area152_FrameStep, bof3::addr::Area152_FrameHold, bof3::addr::Area152_BoxStep,
    bof3::addr::Area152_DrawFrame, bof3::addr::Area152_DrawSprite, bof3::addr::Area152_DrawHud,
};

// --- the other areas' own ---------------------------------------------------------
// kA153Pairs: now Area153_ChoicePairs (symbols.toml). (u8, u8) by the s8 answer: area 153's choice 0
// kA154Pairs: now Area154_ChoicePairs (symbols.toml). area 154's choice 0
constexpr U kA166Pairs = 0x63A79C;        // area 166's choice 1
constexpr U kA167Choices = 0x63C510;      // 16: area 167's +0x34, running on into its handlers (choice 5 = handler 0)
constexpr U kA167Handlers = 0x63C524;     // 11: its +0x3C
constexpr U kA167Switch = 0x63C594;       // (x, z, facing in the low nibble, flag) x 1: the cell hook's
constexpr U kA167SwitchEnd = 0x63C598;
constexpr U kTail32Index = 0x426440;      // 33 bytes in the code: tail kind 32's case by state (0..0xD)
constexpr unsigned kTail32States = 0x21;

}  // namespace at
}  // namespace area_w4a
