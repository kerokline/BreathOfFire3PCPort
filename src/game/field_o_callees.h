// Internal to field_o.cpp and field_o_fuzz.cpp: the cells and image tables
// group FO's functions touch that symbols.toml has no name for. Each a
// load-bearing constant (CLAUDE.md rule 3). docs/field_o.md.
//
// FO calls no other group's function raw: every callee of its 41 is ours
// already or Capcom's by name (the band's two inbound edges, FC1's 0x46A600
// into EventOp_6x and FS's 0x581300 into Menu_DrawTile16, are theirs to bind).
#pragma once

#include <cstdint>

namespace field_o {
namespace at {

using U = std::uint32_t;

// --- the character records the menu panels draw (CharacterRecords, 0xA4 each) ---
constexpr U kRecordStride = 0xA4;
// kStyle: now BattleDraw_Colour (symbols.toml). u8: the window style Menu_DrawBox takes last (the Config byte)
// kShownMember: now BattleActorCopies_ItemActor (symbols.toml). s8: the menu block's +6, the character a panel shows (a party slot)
// kPartyList: now Cond_Flags[0xd2] (symbols.toml). u8 x 3: the party's member ids by slot (Cond_Flags' party list)

// --- the item panel's lists (scratch in .data, shared with chapters 15 / 17's
//     bytes at 0x6BC740: Scena15_Bytes names the first 8) ---
constexpr U kListIds = 0x6BC760;         // u8 x 0x80: the ids the item panel keeps
constexpr U kListCounts = 0x6BC7E0;      // u8 x 0x80: their counts
constexpr U kWheelSpots = 0x6BC748;      // 3 x (x, z) s32: the icon wheel's turned spots (sorted by z)
constexpr U kWheelFrame = 0x6BC860;      // 3 x (x, z) s32: the icon wheel's turned triangle

// --- image tables the panels read in place (never written) ---
constexpr U kStatLabels = 0x66A0F8;      // 4 x 8-byte labels (the four stats), 8 apart
constexpr U kFmtNumber = 0x64E324;       // a number's format (Crt_sprintf)
constexpr U kFmtExp = 0x6639C4;          // the EXP value's format
constexpr U kFmtExpNone = 0x6639B8;      // the text when Char_ExpForLevel answers -1
constexpr U kExpLabel = 0x663670;        // the EXP panel's label, 4 bytes; kExpLabel + 4 the other
constexpr U kWheelNames = 0x6636B0;      // 28-byte entries by kind: the name (Text_DrawSmall)
constexpr U kWheelIcons = 0x6636C0;      // 28-byte entries by kind: three (x, z) s16 pairs
constexpr U kWheelTriangle = 0x6637C8;   // 3 x (x, z) s32: the triangle's corners
constexpr U kEquipIcons = 0x66386C;      // u8 x 6: an icon per equipment slot
constexpr U kAbilityTitles = 0x663984;   // const char * by the panel's +0xB
constexpr U kItemTitles = 0x663994;      // const char * by the category
constexpr U kFmtCount = 0x6639C8;        // the item panel's "n / 128" format
constexpr U kFmtSlot = 0x6639D4;         // the save slot's number format
// kFmtLevel: now Boss26Fx_CountFormat (symbols.toml). the save slot's level format (Boss26Fx_CountFormat's bytes)
constexpr U kFmtTime = 0x654830;         // the save slot's play time format (hours, minutes)
constexpr U kColon = 0x6639D0;           // the save slot's colon text
constexpr U kEmptySlot = 0x669CCC;       // the empty slot's text
constexpr U kNameInsetSite = 0x576A48;   // the disp8 of `lea eax, [ebp + 0x13]` at 0x576A46: DIV-0029's byte
constexpr U kTextScratch = 0x904BA0;     // the text scratch Crt_sprintf and the slot's name copy write

// --- Menu_DrawPieces / Menu_DrawPiece lists the panels draw (image) ---
// kStatsPiecesA: now AreaMap_EntryHandlers[0x2f] (symbols.toml).
// kStatsPiecesB: now AreaMap_EntryHandlers[0x3a] (symbols.toml).
// kStatsPiecesC: now AreaMap_EntryHandlers[0x3d] (symbols.toml).
constexpr U kExpPieces = 0x6634CC;
// kWheelPiecesA: now Menu_PlayTimePieces[0x2c] (symbols.toml).
// kWheelPiecesB: now AreaMap_EntryHandlers[0x5a] (symbols.toml).
// kWheelPiecesC: now AreaMap_EntryHandlers[0x5d] (symbols.toml).
// kEquipPieces: now AreaMap_EntryHandlers[0x6f] (symbols.toml).
// kListPiecesA: now AreaMap_EntryHandlers[0x7a] (symbols.toml).
constexpr U kListPiecesLit = 0x66349C, kListPiecesB = 0x6634B4, kListPiecesBLit = 0x6634C0;
// kItemPiecesB: now AreaMap_EntryHandlers[0x7d] (symbols.toml).
constexpr U kSlotPieces = 0x663584;

// --- the movement script's tables read in place ---
// kOp88States: now MoveCmd_Op88States (symbols.toml). 2 handlers (MoveCmd_Op88), then kOp87States' 2
// kOp87States: now MoveCmd_Op87States (symbols.toml). 2 handlers (MoveCmd_Op87), then EventScript_OpLengths
// kOpE9States: now MoveCmd_OpE9States (symbols.toml). 4 handlers of seven words (MoveCmd_OpE9)
// kDirectionVelocity: now MoveCmd_F9Steps (symbols.toml). 8 x (x, z) s32 by direction: MoveCmd_OpF9's step per unit of speed

// .text, for the table reads: an entry outside it is not code.
constexpr U kTextLo = 0x401000, kTextHi = 0x5C3000;

}  // namespace at
}  // namespace field_o
