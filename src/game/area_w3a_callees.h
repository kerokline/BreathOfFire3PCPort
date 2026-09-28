// Internal to area_w3a.cpp and area_w3a_fuzz.cpp: the cells and tables world
// 3's areas 115..119 touch that symbols.toml has no name for (the tables it
// names are listed too, by address, for the fuzz's regions), the world-map
// copy's table set and areas 117 / 118's twin table sets, and the one callee
// nobody owns this wave, by raw address. docs/area_w3a.md.
//
// Raw-address callee (the round's rebinding pass names it):
//   0x4220D0  (const long* point): reads the point's three dwords (x, z, y)
//             and builds positions about it (docs/area_w2d.md's reading, its
//             first 0x100 bytes); world 3's area code in group AR3F's band
//             this wave (the tool's AREA146).
#pragma once

#include <cstdint>

#include "bof3/symbols.gen.h"

// Rebound 2026-09-28 (round ten's cleanup, docs/round-10-cleanup.md item 1):
// every constant here whose target is ours reads bof3::addr::<Name>. The values
// are unchanged - the fuzz keys on them - and the comments' "nobody owns" is as
// of the wave that wrote them.
namespace area_w3a {
namespace at {

using U = std::uint32_t;

// --- the field frame's cells ------------------------------------------------
constexpr U kStoryFlags = 0x904030;       // the flag bank every Flags_* call here is handed
constexpr U kTailKind = 0x9039F3;         // Field_ModeTailKinds' index (s8)
constexpr U kTailState = 0x9039F4;        // s8: the armed tail kind's (or the place hook's) state
constexpr U kTailArg = 0x9039F5;
constexpr U kLeader = 0x802D40;           // ObjTrio + 0, the leader's record
constexpr U kLeaderDir = 0x802D48;        // the leader's +8, its facing
constexpr U kLeaderX = 0x802D74;          // the leader's +0x34 / +0x38, 16.16
constexpr U kLeaderZ = 0x802D78;
constexpr U kPartyStride = 0x14C;
constexpr U kPartyList = 0x904062;        // the first party list: the members' ids, a byte each
constexpr U kCursor = 0x7DEE67;           // the message box's cursor (s8): a choice's answer
constexpr U kMessage = 0x7DEE48;          // u16: the message a choice opened, 0xFFFF none
constexpr U kCounter0 = 0x903848;         // MoveScript counter 0 (flow ops A0..AC)
constexpr U kByte9398CF = 0x9398CF;       // set to 6 by the choices that refuse (area_w0a's kByte9398CF)
constexpr U kZenny = 0x904058;            // Party_Zenny: u32
constexpr U kFlagRow = 0x929ED0;          // the chapter's Cond_Flags row (a pointer, Scenario_Enter)
constexpr U kFieldStateMember = 0x128;    // the record byte the member frames set to 2

// --- the world maps' cells (area 16's, docs/area_w0b.md; as area_w2b's) --------
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
constexpr U kAreaText = 0x803580;         // the area's text section; area 115 reads its label offset from its first dword
constexpr U kItemsHeld = 0x9040EC;        // a byte per item id: 0 not yet seen
constexpr U kItemNameKey = 0x669CD8;      // the 16-byte name the hook copies for item 0x16
constexpr U kTextRecords = 0x904CE0;      // Text_Records: the place hook fills rows of 0x20 from here
constexpr U kFlag3A79 = 0x903A79;         // the record +4 state: 9 releases the effect
constexpr U kMsgMode = kTailKind, kMsgState = kTailState, kMsgArg = kTailArg;

// --- a world-map copy's tables (area 45's layout, docs/area_w1b.md section 5;
// area_w2b_callees.h's struct, the same fields) ---------------------------------
struct WorldMapTables {
    const char* name;                 // "Area115": the messages' prefix
    U plate_anims, plate_anims_end;   // (u16 place, u8 animation, u8), searched with NO bound
    U cells, cells_end;               // (u8 x, u8 z, u8, u8 id): the shipped records, searched with NO bound
    U place_messages, place_messages_end;   // rows of 0x20: u16 place, fifteen u16 messages
    U name_sets, name_sets_end;       // (u8 id, u8 item x (set_stride - 1))
    unsigned set_stride;              // 6 (area 115)
    U text_rows_end;                  // the place hook's last Text_Records row (exclusive)
    U plate_states, hud_states, frame_states, box_states;
    U sprites, buttons;               // (w, h, u, v) x 22; (u16 mask, u8 sprite, u8) x 6, the second legend reads eight
    U record8_states, directions, record8_anims, record4_states;
    U drift_u, drift_v, drift_size;   // u8 by +0xB - 2 each
    U label_offset;                   // the dword whose low word is the region label's offset in kAreaText
    unsigned plate_bank;              // PlateStart's Sprite_SetAnimationBank
    // the copy's own functions another of them calls directly
    U fn_frame_step, fn_frame_hold, fn_box_step, fn_draw_frame, fn_draw_sprite, fn_draw_hud;
};

// Area 115: WorldMap_Records record 7 (0x6539D4), WorldMap_FieldHooks entry 7;
// its descriptor 0x620220 (only a choice table, +0x34 -> 0x620218, whose one
// entry is 0x41D270 - group AR3C's band - and no init, as area 88's).
constexpr WorldMapTables kWm115 = {
    "Area115",
    0x620148, 0x62015C,   // 5 plate animations
    0x62015C, 0x620164,   // 2 cell records (then a zero record, then the descriptor's data)
    0x620264, 0x620304,   // 5 place rows
    0x620304, 0x620310, 6, 0x904D80,   // 2 name sets of (id, 5 items); 5 text rows
    0x620310, 0x620324, 0x62032C, 0x62033C,
    0x62034C, 0x6203A4,
    0x6203BC, 0x6203C8, 0x6203D8, 0x6203E0,
    0x6203E8, 0x6203EC, 0x6203F0,
    0x803580, 0x1C7,
    bof3::addr::Area115_FrameStep, bof3::addr::Area115_FrameHold, bof3::addr::Area115_BoxStep,
    bof3::addr::Area115_DrawFrame, bof3::addr::Area115_DrawSprite, bof3::addr::Area115_DrawHud,
};

// --- area 116's (its descriptor 0x620620) -----------------------------------------
constexpr U kA116EffectStates = 0x620664;    // 2: Area59_EffectGround 0x40B4F0 (group AR1D's), Area116_EffectB8Ring
constexpr unsigned kA116EffectStateCount = 2;

// --- areas 117 and 118: one body over a table set each ---------------------------
struct TwinTables {
    const char* name;                 // "Area117"
    U rects, rects_end;               // (x0, z0, x1, z1, facing, flag) x n: MemberRect's search, bounded
    U switches, switches_end;         // (x, z, facing, flag) x n: the cell hook's search, bounded
    unsigned switch_count;            // the hook's "not found" test is the index == this count
    U fn_member_rect;                 // the copy's MemberRect, called by its MembersFrame
};
// Area 117: its descriptor 0x6215B0; the rectangle 0x6215A8, the switch 0x62160C.
constexpr TwinTables kTw117 = {"Area117", 0x6215A8, 0x6215AE, 0x62160C, 0x621610, 1, bof3::addr::Area117_MemberRect};
// Area 118: its descriptor 0x621DF8; the rectangle 0x621DF0, the switch 0x621E3C.
constexpr TwinTables kTw118 = {"Area118", 0x621DF0, 0x621DF6, 0x621E3C, 0x621E40, 1, bof3::addr::Area118_MemberRect};

// --- the member-spawn handlers' tables (12 bytes each, by a member's id) -------
constexpr U kSpawnTable117A = 0x620670;   // area 117's handler 0
constexpr U kSpawnTable117B = 0x62067C;   // handlers 1 and 2
constexpr U kSpawnTable117C = 0x620688;   // handler 3
constexpr U kSpawnTable118 = 0x621610;    // area 118's handler 0
constexpr U kSpawnTable119A = 0x621E40;   // area 119's handlers 0, 4, 9
constexpr U kSpawnTable119B = 0x621E4C;   // handlers 5..8

// --- area 117's choice message tables (u16 by the cursor, s8, unchecked) -------
constexpr U kA117Messages1 = 0x6215F4;
constexpr U kA117Messages2 = 0x621600;

// --- the member-spawn handlers' jump table in area 117's choice 2 -------------
constexpr U kA117Choice2Cases = 0x41A080;   // 5 entries, in the function's own extent

}  // namespace at

// The unowned callee (above).
constexpr std::uint32_t kRingAt = bof3::addr::Area146_DrawGlowCylinder;

}  // namespace area_w3a
