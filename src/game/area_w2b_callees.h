// Internal to area_w2b.cpp and area_w2b_fuzz.cpp: the cells and tables world
// 2's areas 85..88 touch that symbols.toml has no name for (the tables it
// names are listed too, by address, for the fuzz's regions), the world-map
// copies' table sets, and the callees nobody owns this wave, by raw address.
// docs/area_w2b.md.
//
// Raw-address callees (the round's rebinding pass names them):
//   0x454A80  (object): releases every Field_Slots record whose +0xC is the
//             object (0x454A50 each); engine code nobody owns (read
//             2026-09-28, 0x454A80..0x454AAA).
//   0x455290  (object, script): the first free Field_Slots record (+0 bit 0
//             clear) of eight gets +0 = 1, +0xC the object, +4 the script,
//             +2 = 0xFF, +3 = object +0x27; al its index, 0xFF none; engine
//             code nobody owns (read 2026-09-28, 0x455290..0x4552F5).
//   0x57C160  (bits, index): toggles bit (index & 7) of bits[(index & 0xFF)
//             >> 3] (docs/area_w1b.md); SX2's this wave.
//   0x469FE0  (v): Effect_FindFree; a free slot gets +0 = 1, kind +5 = 4, +9
//             = v, and story flag 0x1C is set (docs/area_w1c.md); SX2's this
//             wave.
#pragma once

#include <cstdint>

namespace area_w2b {
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
constexpr U kEffectStride = 0x80;
constexpr U kScriptVar7 = 0x8034E4;       // MoveScript_Var7 (s8): the running scene's run
constexpr U kScriptStep = 0x8034E5;       // the run's step (Scena* step machines)
constexpr U kPrevArea = 0x802290;         // u16: the area the party came from (Area_Enter, docs/mode-flow.md)
constexpr U kSlots = 0x9035C0;            // Field_Slots: 8 records of 0x10
constexpr U kClutRow6 = 0x80C180;         // Gfx_ClutStripSource + 0xC00: CLUT row 6 as loaded (0x100 words)
constexpr U kClutRow6Live = 0x810180;     // Gfx_ClutStrip + 0xC00: the row the game uploads

// --- area 85's ------------------------------------------------------------------
constexpr U kA85Handlers = 0x61177C;      // 10: its descriptor 0x6117A8's +0x3C (entry 7 shared with area 198)
constexpr U kA85SlotScript = 0x611814;    // the movement script handler 6 hands 0x455290

// --- area 86's (its descriptor 0x611C38) ----------------------------------------
constexpr U kA86Switches = 0x611C7C;      // (x, z, facing, flag) x 3: the cell hook's
constexpr U kA86SwitchesEnd = 0x611C88;
constexpr U kA86Rects = 0x611C88;         // (x0, z0, x1, z1) x 3 by the switch: the cells that must be clear
constexpr U kA86RectsEnd = 0x611C94;

// --- the world maps' cells (area 16's, docs/area_w0b.md) ----------------------
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
constexpr U kAreaText = 0x803580;         // the area's text section; areas 87 and 88 read their label offset from its first dword
constexpr U kItemsHeld = 0x9040EC;        // a byte per item id: 0 not yet seen
constexpr U kItemNameKey = 0x669CD8;      // the 16-byte name the hook copies for item 0x16
constexpr U kTextRecords = 0x904CE0;      // Text_Records: the place hook fills rows of 0x20 from here
constexpr U kFlag3A79 = 0x903A79;         // the record +4 state: 9 releases the effect
constexpr U kMsgMode = kTailKind, kMsgState = kTailState, kMsgArg = kTailArg;

// --- a world-map copy's tables (area 45's layout, docs/area_w1b.md section 5;
// each area's in its data block after the previous area's descriptor) ----------
struct WorldMapTables {
    const char* name;                 // "Area87": the messages' prefix
    U plate_anims, plate_anims_end;   // (u16 place, u8 animation, u8), searched with NO bound
    U cells, cells_end;               // (u8 x, u8 z, u8, u8 id): the shipped records, searched with NO bound
    U place_messages, place_messages_end;   // rows of 0x20: u16 place, fifteen u16 messages
    U name_sets, name_sets_end;       // (u8 id, u8 item x set_items)
    unsigned set_stride;              // 5 (area 87), 12 (area 88)
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

// Area 87: WorldMap_Records record 4 (0x653980), WorldMap_FieldHooks entry 4;
// its descriptor 0x612488 (only a choice table, +0x34 -> 0x41D270, and the
// init 0x40FC40, which area 65 shares).
constexpr WorldMapTables kWm87 = {
    "Area87",
    0x611C98, 0x611CC0,   // 10 plate animations
    0x611CC0, 0x611CCC,   // 3 cell records (then a zero record, then the descriptor's data)
    0x6124CC, 0x61260C,   // 10 place rows
    0x61260C, 0x61261B, 5, 0x904D60,   // 3 name sets of (id, 4 items); 4 text rows
    0x61261C, 0x612630, 0x612638, 0x612648,
    0x612658, 0x6126B0,
    0x6126C8, 0x6126D4, 0x6126E4, 0x6126EC,
    0x6126F4, 0x6126F8, 0x6126FC,
    0x803580, 0x156,
    0x4101A0, 0x4101F0, 0x410270, 0x4103D0, 0x4105A0, 0x410660,
};

// Area 88: record 5 (0x65399C), field hook entry 5; its descriptor 0x612E50
// (only a choice table, +0x34 -> 0x41D270).
constexpr WorldMapTables kWm88 = {
    "Area88",
    0x612700, 0x61271C,   // 7 plate animations
    0x61271C, 0x612728,   // 3 cell records (then the descriptor's data)
    0x612E94, 0x612F74,   // 7 place rows
    0x612F74, 0x612F98, 12, 0x904E40,   // 3 name sets of (id, 11 items); 11 text rows
    0x612F98, 0x612FAC, 0x612FB4, 0x612FC4,
    0x612FD4, 0x61302C,
    0x613044, 0x613050, 0x613060, 0x613068,
    0x613070, 0x613074, 0x613078,
    0x803580, 0x157,
    0x4112F0, 0x411370, 0x4113F0, 0x411550, 0x411720, 0x4117E0,
};

}  // namespace at

// The unowned callees (above).
constexpr std::uint32_t kSlotsReleaseFor = 0x454A80;
constexpr std::uint32_t kSlotStart = 0x455290;
constexpr std::uint32_t kFlagsToggle = 0x57C160;
constexpr std::uint32_t kSpawnKind4 = 0x469FE0;

}  // namespace area_w2b
