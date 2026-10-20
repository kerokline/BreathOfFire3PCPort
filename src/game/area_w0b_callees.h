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

#include "bof3/symbols.gen.h"
// Rebound 2026-09-28 (round ten's cleanup, docs/round-10-cleanup.md item 1):
// every constant here whose target is ours reads bof3::addr::<Name>. The values
// are unchanged - the fuzz keys on them - and the comments' "nobody owns" is as
// of the wave that wrote them.

namespace area_w0b {
namespace at {

using U = std::uint32_t;

// --- the field frame's cells ------------------------------------------------
// kLeader: now ObjTrio (symbols.toml). ObjTrio + 0, the leader's record
// kLeaderSteps: now ObjTrio[0].step_frames (symbols.toml). u8 step count
// kLeaderDirX: now ObjTrio[0].step_x (symbols.toml). dword direction
// kLeaderDirZ: now ObjTrio[0].step_z (symbols.toml).
// kLeaderX: now ObjTrio[0].x (symbols.toml). 16.16 position; its high words are the cell
// kLeaderZ: now ObjTrio[0].z (symbols.toml).
// kLeaderCellWordX: now ObjTrio[0].x_whole (symbols.toml). the high word of +0x34
// kLeaderCellWordZ: now ObjTrio[0].z_whole (symbols.toml). the high word of +0x38
// kLeaderEdge: now ObjTrio_addr + 0x134 (symbols.toml). the leader's dword +0x134 (Field_EdgeBits = it - 5)
// kPlace: now AreaW0b_Place (symbols.toml). u16: the place the party stands on (world map)
// kMsgMode: now AreaW0a_TailKind (symbols.toml). the tail kind byte (Field_ModeTailKinds' index)
// kMsgState: now AreaW0c_TailState (symbols.toml). s8: the field hook's state
// kMsgArg: now AreaW0c_TailSub (symbols.toml).
// kMapMode: now AreaW0b_MapMode (symbols.toml). the world map's mode byte
constexpr U kLeaderCellX = 0x905E66;      // the high words of Field_Kind2X / Z
constexpr U kLeaderCellZ = 0x905E62;
// kMapHeight: now AreaMap_Header[1] (symbols.toml). AreaMap_Header's second byte: the map's height in cells
// kButtonMap0: now Gfx_UnpackScratch[0x1f000] (symbols.toml). the field's button map: word 0, the first legend's button
// kButtonMap6: now Gfx_UnpackScratch[0x1f00c] (symbols.toml). word 6, the second's
// kPartySet: now AreaW0b_PartySet (symbols.toml). & 0x7F: the party set (0xC shows the third legend)
constexpr U kAreaText = 0x803580;         // the area's text section: the region label at +[0x803588] & 0xFFFF
constexpr U kAreaTextOffset = 0x803588;
constexpr U kItemsHeld = 0x9040EC;        // a byte per item id: 0 not yet seen (the hook's list shows "????????")
// kItemNameKey: now AreaW0b_ItemNameKey (symbols.toml). the 16-byte name the hook copies for item 0x16 (no Item_NamePtr call)
// kMoney: now Party_Zenny (symbols.toml). dword
// kChoice: now AreaW0c_ChoiceAnswer (symbols.toml). s8: the choice the message box committed
constexpr U kMessage = 0x7DEE48;          // u16: the message a choice handler opens, 0xFFFF none
constexpr U kCounter = 0x90384B;          // area 21's trade counter, 0..7
constexpr U kScriptVar = 0x903848;        // a movement-script variable byte (the choices set it)
constexpr U kEffectCount = 0x90384A;      // area 25: its effects' count byte (below 0x14: variant 1)
inline auto& kFoundSlot = SharedScratch.b_00;  // area 25: the slot Effect_FindFree answered
constexpr U kFlagBase = 0x929ED0;         // the pointer Flags_Set / Flags_Clear are handed (a flag row)
constexpr U kTextRecords = 0x904CE0;      // Text_Records: area 16's hook fills its first four rows of 0x20
constexpr U kTextRecordsEnd = 0x904D60;
constexpr U kTextRow1 = 0x904D00;         // Text_Records + 0x20: area 21's number, by Crt_sprintf
constexpr U kFormat = 0x5E10C0;           // the "%d"-shaped format it uses (read-only)
constexpr U kFlag3A79 = 0x903A79;         // area 16's record +4 state: 9 releases the effect
// kPartyFirst: now Cond_Flags[0xd2] (symbols.toml). the first party list's first member id
constexpr U kAreaRecords = 0x668D80;      // a pointer per area; area 18's init writes the byte +4 of its own

// --- area 16's tables (the world-map copy's, in its data block after area
// 17's descriptor; tools/area_rows.py moves them to area 16) -----------------
// kA16PlateAnims: now Area16_PlateAnims (symbols.toml). (u16 place, u8 animation, u8) x 9, searched with NO bound
// kA16Cells: now Area16_Cells (symbols.toml). (u8 x, u8 z, u8, u8 id) x 138, searched with NO bound
constexpr U kA16CellsEnd = 0x5E5E2C;
// kA16PlaceMessages: now Area16_PlaceMessages (symbols.toml). 9 rows of 0x20: u16 place, fifteen u16 messages
constexpr U kA16PlaceMessagesEnd = 0x5E5F4C;
// kA16NameSets: now Area16_NameSets (symbols.toml). 3 records of (u8 id, u8 item x 4)
constexpr U kA16NameSetsEnd = 0x5E5F5B;
// kA16PlateStates: now Area16_PlateStates (symbols.toml). 5 handlers
// kA16HudStates: now Area16_HudStates (symbols.toml). 2
// kA16FrameStates: now Area16_FrameStates (symbols.toml). 4
// kA16BoxStates: now Area16_BoxStates (symbols.toml). 4
// kA16Sprites: now Area16_Sprites (symbols.toml). (w, h, u, v) x 22, by index & 0xFF unchecked
// kA16Buttons: now Area16_Buttons (symbols.toml). (u16 mask, u8 sprite, u8) x 6; the second legend reads EIGHT
constexpr U kA16Buttons6End = 0x5E6008;
constexpr U kA16Buttons8End = 0x5E6010;
// kA16Record8States: now Area16_Record8States (symbols.toml). 3 (0x4253C0 and 0x40C490 other areas' shared ones)
// kA16Directions: now Area16_Directions (symbols.toml). (s16 dx, s16 dz) x 4 by +8, unchecked
// kA16Record8Anims: now Area16_Record8Anims (symbols.toml). (u8 animation, u8 +0x2A) x 4 by +8, unchecked
// kA16Record4States: now Area16_Record4States (symbols.toml). 2 (0x408990 another area's shared one)
// kA16DriftUBase: now Area16_DriftUV (symbols.toml). u8 by +0xB - 2; v base +4, cell size +8
// kA16DriftVBase: now Area16_DriftUV[4] (symbols.toml).
// kA16DriftSize: now Area16_DriftUV[8] (symbols.toml).

// --- the other areas' tables --------------------------------------------------
// kA20Cells: now Area20_Cells (symbols.toml). (u8 x, u8 z) x 8
// kA20Weights: now Area20_Weights (symbols.toml). u8 x 8, summing to 64
// kA21Messages: now Area21_Messages (symbols.toml). u16 by the s8 choice, unchecked
// kA26Messages: now Area26_Messages (symbols.toml). u16 by the s8 choice, unchecked
// kA26EffectKinds: now Area26_EffectKinds (symbols.toml). u8 by the first member id, unchecked

}  // namespace at

// The unowned callees (above).
constexpr std::uint32_t kMoneyTake = bof3::addr::Zenny_Sub;
constexpr std::uint32_t kMoneyGive = bof3::addr::Zenny_Add;
constexpr std::uint32_t kInventoryTake = bof3::addr::Inventory_Remove;

}  // namespace area_w0b
