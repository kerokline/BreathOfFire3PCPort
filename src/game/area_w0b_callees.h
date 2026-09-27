// Internal to area_w0b.cpp and area_w0b_fuzz.cpp: the cells and tables world
// 0's areas 16 and 18..26 touch that symbols.toml has no name for (the tables
// it names are listed too, by address, for the fuzz's regions), and the three
// callees nobody owns, by raw address. docs/area_w0b.md.
//
// Raw-address callees (engine code no group owns this wave; the round's
// rebinding pass names them):
//   0x591BC0  the money take: (amount) - if the dword 0x904058 is at least the
//             amount it is that much less and al 1, else al 0
//   0x591BE0  the money give: (amount, flag) - 0x904058 += amount, and
//             0x904138 too when the flag byte is 0; capped at 9,999,999 (al 0)
//   0x591B60  the inventory take: (category, item, count) - the item's count
//             in its category's lists less count (the slot cleared at 0); al
//             1, or 0 when the item is not held or has fewer
#pragma once

#include <cstdint>

namespace area_w0b {
namespace at {

using U = std::uint32_t;

// --- the field frame's cells ------------------------------------------------
constexpr U kLeader = 0x802D40;           // ObjTrio + 0, the leader's record
constexpr U kLeaderSteps = 0x802D49;      // u8 step count
constexpr U kLeaderDirX = 0x802D4C;       // dword direction
constexpr U kLeaderDirZ = 0x802D50;
constexpr U kLeaderX = 0x802D74;          // 16.16 position; its high words are the cell
constexpr U kLeaderZ = 0x802D78;
constexpr U kLeaderCellWordX = 0x802D76;  // the high word of +0x34
constexpr U kLeaderCellWordZ = 0x802D7A;  // the high word of +0x38
constexpr U kLeaderEdge = 0x802E74;       // the leader's dword +0x134 (Field_EdgeBits = it - 5)
constexpr U kPlace = 0x937F82;            // u16: the place the party stands on (world map)
constexpr U kMsgMode = 0x9039F3;          // the tail kind byte (Field_ModeTailKinds' index)
constexpr U kMsgState = 0x9039F4;         // s8: the field hook's state
constexpr U kMsgArg = 0x9039F5;
constexpr U kMapMode = 0x9045FA;          // the world map's mode byte
constexpr U kLeaderCellX = 0x905E66;      // the high words of Field_Kind2X / Z
constexpr U kLeaderCellZ = 0x905E62;
constexpr U kMapHeight = 0x8CB581;        // AreaMap_Header's second byte: the map's height in cells
constexpr U kButtonMap0 = 0x903580;       // the field's button map: word 0, the first legend's button
constexpr U kButtonMap6 = 0x90358C;       // word 6, the second's
constexpr U kPartySet = 0x90412C;         // & 0x7F: the party set (0xC shows the third legend)
constexpr U kAreaText = 0x803580;         // the area's text section: the region label at +[0x803588] & 0xFFFF
constexpr U kAreaTextOffset = 0x803588;
constexpr U kItemsHeld = 0x9040EC;        // a byte per item id: 0 not yet seen (the hook's list shows "????????")
constexpr U kItemNameKey = 0x669CD8;      // the 16-byte name the hook copies for item 0x16 (no Item_NamePtr call)
constexpr U kMoney = 0x904058;            // dword
constexpr U kChoice = 0x7DEE67;           // s8: the choice the message box committed
constexpr U kMessage = 0x7DEE48;          // u16: the message a choice handler opens, 0xFFFF none
constexpr U kCounter = 0x90384B;          // area 21's trade counter, 0..7
constexpr U kScriptVar = 0x903848;        // a movement-script variable byte (the choices set it)
constexpr U kEffectCount = 0x90384A;      // area 25: its effects' count byte (below 0x14: variant 1)
constexpr U kFoundSlot = 0x903850;        // area 25: the slot Effect_FindFree answered
constexpr U kFlagBase = 0x929ED0;         // the pointer Flags_Set / Flags_Clear are handed (a flag row)
constexpr U kTextRecords = 0x904CE0;      // Text_Records: area 16's hook fills its first four rows of 0x20
constexpr U kTextRecordsEnd = 0x904D60;
constexpr U kTextRow1 = 0x904D00;         // Text_Records + 0x20: area 21's number, by Crt_sprintf
constexpr U kFormat = 0x5E10C0;           // the "%d"-shaped format it uses (read-only)
constexpr U kFlag3A79 = 0x903A79;         // area 16's record +4 state: 9 releases the effect
constexpr U kPartyFirst = 0x904062;       // the first party list's first member id
constexpr U kAreaRecords = 0x668D80;      // a pointer per area; area 18's init writes the byte +4 of its own

// --- area 16's tables (the world-map copy's, in its data block after area
// 17's descriptor; tools/area_rows.py moves them to area 16) -----------------
constexpr U kA16PlateAnims = 0x5E5BE0;    // (u16 place, u8 animation, u8) x 9, searched with NO bound
constexpr U kA16Cells = 0x5E5C04;         // (u8 x, u8 z, u8, u8 id) x 138, searched with NO bound
constexpr U kA16CellsEnd = 0x5E5E2C;
constexpr U kA16PlaceMessages = 0x5E5E2C; // 9 rows of 0x20: u16 place, fifteen u16 messages
constexpr U kA16PlaceMessagesEnd = 0x5E5F4C;
constexpr U kA16NameSets = 0x5E5F4C;      // 3 records of (u8 id, u8 item x 4)
constexpr U kA16NameSetsEnd = 0x5E5F5B;
constexpr U kA16PlateStates = 0x5E5F5C;   // 5 handlers
constexpr U kA16HudStates = 0x5E5F70;     // 2
constexpr U kA16FrameStates = 0x5E5F78;   // 4
constexpr U kA16BoxStates = 0x5E5F88;     // 4
constexpr U kA16Sprites = 0x5E5F98;       // (w, h, u, v) x 22, by index & 0xFF unchecked
constexpr U kA16Buttons = 0x5E5FF0;       // (u16 mask, u8 sprite, u8) x 6; the second legend reads EIGHT
constexpr U kA16Buttons6End = 0x5E6008;
constexpr U kA16Buttons8End = 0x5E6010;
constexpr U kA16Record8States = 0x5E6008; // 3 (0x4253C0 and 0x40C490 other areas' shared ones)
constexpr U kA16Directions = 0x5E6014;    // (s16 dx, s16 dz) x 4 by +8, unchecked
constexpr U kA16Record8Anims = 0x5E6024;  // (u8 animation, u8 +0x2A) x 4 by +8, unchecked
constexpr U kA16Record4States = 0x5E602C; // 2 (0x408990 another area's shared one)
constexpr U kA16DriftUBase = 0x5E6034;    // u8 by +0xB - 2; v base +4, cell size +8
constexpr U kA16DriftVBase = 0x5E6038;
constexpr U kA16DriftSize = 0x5E603C;

// --- the other areas' tables --------------------------------------------------
constexpr U kA20Cells = 0x5E6C3C;         // (u8 x, u8 z) x 8
constexpr U kA20Weights = 0x5E6C4C;       // u8 x 8, summing to 64
constexpr U kA21Messages = 0x5E72C4;      // u16 by the s8 choice, unchecked
constexpr U kA26Messages = 0x5EC5E4;      // u16 by the s8 choice, unchecked
constexpr U kA26EffectKinds = 0x5EC5E8;   // u8 by the first member id, unchecked

}  // namespace at

// The unowned callees (above).
constexpr std::uint32_t kMoneyTake = 0x591BC0;
constexpr std::uint32_t kMoneyGive = 0x591BE0;
constexpr std::uint32_t kInventoryTake = 0x591B60;

}  // namespace area_w0b
