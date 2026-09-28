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

namespace area_w3g {
namespace at {

using U = std::uint32_t;

// --- the field frame's cells ------------------------------------------------
constexpr U kStoryFlags = 0x904030;       // the flag bank most Flags_* calls here are handed
constexpr U kFlags904000 = 0x904000;      // the bank areas 149 and 150 test (flags 0x11, 3)
constexpr U kTailKind = 0x9039F3;         // Field_ModeTailKinds' index (s8)
constexpr U kTailState = 0x9039F4;        // s8: the armed tail kind's (or the place hook's) state
constexpr U kTailArg = 0x9039F5;          // the tail's byte: area 148 keeps an effect slot in it
constexpr U kTailTimer = 0x9039F6;        // u16: the tails' frame counter
constexpr U kLeader = 0x802D40;           // ObjTrio + 0, the leader's record
constexpr U kLeaderDir = 0x802D48;        // the leader's +8, its facing
constexpr U kLeaderX = 0x802D74;          // the leader's +0x34 / +0x38, 16.16
constexpr U kLeaderZ = 0x802D78;
constexpr U kPartyStride = 0x14C;
constexpr U kMemberId = 0x802DC9;         // party record 0's +0x89: the member's id
constexpr U kCursor = 0x7DEE67;           // the message box's cursor (s8): a choice's answer
constexpr U kMessage = 0x7DEE48;          // u16: the message a choice opened, 0xFFFF none
constexpr U kCounter0 = 0x903848;         // MoveScript counter 0 (flow ops A0..AC)
constexpr U kScriptVar = 0x90384B;        // the movement script's byte the tails wait on
constexpr U kCameraAngle1 = 0x929ECA;     // s16 Camera_Angles[1]
constexpr U kCameFrom = 0x802290;         // u16: the area the party came from (docs/mode-flow.md)
constexpr U kEffects = 0x7E11E0;          // Effect_Objects: records of 0x80
constexpr U kEffectStride = 0x80;
constexpr unsigned kEffectCount = 20;
constexpr U kExtra0 = 0x802000;           // Sprite_ObjectsExtra record 0
constexpr U kKind2X = 0x7E0974;           // Sprite_Kind2 + 0x34 (s32)
constexpr U kKind2Height = 0x7E097E;      // Sprite_Kind2 + 0x3E (u16)
constexpr U kFocusX = 0x929F14;           // MapView_FocusX (dword)
constexpr U kOrigin = 0x7E0688;           // MapView_Origin (its first word)
constexpr U kDirSteps = 0x6697B0;         // Field_DirectionSteps: (x, z) s32 a direction
constexpr U kWalkDelta = 0x6696DC;        // s32 x, z per direction, 8 bytes apart (event_ops_callees.h's)
constexpr U kHealByte = 0x904153;         // area 150's tail stores (docs/area_w3g.md section 4)
constexpr U kWord90405C = 0x90405C;
constexpr U kByte90405E = 0x90405E;
constexpr U kByte90405F = 0x90405F;
constexpr U kByte929EC1 = 0x929EC1;
constexpr U kByte9036D0 = 0x9036D0;

// --- area 148 (descriptor 0x635388) -----------------------------------------
constexpr U kA148BeamColours = 0x6353CC;  // (r, g, b) by the effect's +6
constexpr U kA148MemberIdsA = 0x6353D8;   // 4 member ids, then 4 u16 messages at 0x6353DC
constexpr U kA148MessagesA = 0x6353DC;
constexpr U kA148MemberIdsB = 0x6353E4;   // the same for handler 5
constexpr U kA148MessagesB = 0x6353E8;
constexpr U kA148Switch = 0x6353F0;       // one (x, z, facing, tail state) record: the cell hook's
constexpr U kA148SwitchEnd = 0x6353F4;
constexpr U kA148BeamStates = 0x6353F4;   // effect kind 0x7E's three states
constexpr unsigned kA148BeamStateCount = 3;
constexpr U kA148TurnSteps = 0x635400;    // 10 pairs of s8 indexes by +9 >> 3
constexpr U kA148TurnCells = 0x635414;    // (dx, dz) s8 pairs

// --- area 149 (descriptor 0x6361B8) -----------------------------------------
constexpr U kA149DustStates = 0x6361FC;   // handler 1's four states by Sprite_Current[4]
constexpr unsigned kA149DustStateCount = 4;

// --- area 150 (descriptor 0x636A18) -----------------------------------------
constexpr U kA150MessagesA = 0x636A5C;    // u16 by the s8 cursor (choice 0), unchecked
constexpr U kA150MessagesB = 0x636A60;    // u16 by the s8 cursor (choice 1), unchecked

// --- the world maps' cells (area 16's, docs/area_w0b.md; as area_w3a's) --------
constexpr U kLeaderSteps = 0x802D49;      // u8 step count
constexpr U kLeaderDirX = 0x802D4C;       // dword direction
constexpr U kLeaderDirZ = 0x802D50;
constexpr U kLeaderCellWordX = 0x802D76;  // the high word of +0x34
constexpr U kLeaderCellWordZ = 0x802D7A;  // the high word of +0x38
constexpr U kPlace = 0x937F82;            // u16: the place the party stands on (world map)
constexpr U kMapMode = 0x9045FA;          // the world map's mode byte
constexpr U kLeaderCellX = 0x905E66;      // the high words of Field_Kind2X / Z
constexpr U kLeaderCellZ = 0x905E62;
constexpr U kMapHeight = 0x8CB581;        // AreaMap_Header's second byte
constexpr U kButtonMap0 = 0x903580;       // the field's button map: word 0, the first legend's button
constexpr U kButtonMap6 = 0x90358C;       // word 6, the second's
constexpr U kPartySet = 0x90412C;         // & 0x7F: the party set (0xC shows the third legend)
constexpr U kAreaText = 0x803580;         // the area's text section; area 151 reads its label offset from its first dword
constexpr U kItemsHeld = 0x9040EC;        // a byte per item id: 0 not yet seen
constexpr U kItemNameKey = 0x669CD8;      // the 16-byte name the hook copies for item 0x16
constexpr U kTextRecords = 0x904CE0;      // Text_Records: the place hook fills rows of 0x20 from here
constexpr U kFlag3A79 = 0x903A79;         // the record +4 state: 9 releases the effect
constexpr U kMsgMode = kTailKind, kMsgState = kTailState, kMsgArg = kTailArg;

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
    0x423DE0, 0x423E30, 0x423EB0, 0x424010, 0x4241E0, 0x4242A0,
};

}  // namespace at

// The unowned callees (above).
constexpr std::uint32_t kSetMapCamera = 0x494060;
constexpr std::uint32_t kProjectPoint = 0x494110;
constexpr std::uint32_t kScreenSize = 0x4941E0;
constexpr std::uint32_t kViewShift = 0x56FCA0;

}  // namespace area_w3g
