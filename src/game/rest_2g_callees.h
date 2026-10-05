// Group R2G of round fourteen: the cells, tables and the callees not yet ours
// that rest_2g.cpp names by address (docs/rest_2g.md section 3). Every value
// is a load-bearing address of BOF3.exe.
#pragma once

#include <cstdint>

#include "bof3/symbols.gen.h"

namespace rest_2g {
namespace at {

// The window records (WindowRecords, 22 of 0x24) and the one being run.
constexpr std::uint32_t kWindows = 0x803160;
constexpr std::uint32_t kWindowStride = 0x24;
constexpr unsigned kWindowCount = 22;
constexpr std::uint32_t kCurrent = 0x905B84;          // the record Field_RunTaskRecords is running
constexpr std::uint32_t kRecord4X = 0x8031F4;         // window record 4's +4 (the field menu's first member panel)

// The window style byte (Menu_DrawBox's colour) and the party list the panels read.
constexpr std::uint32_t kColour = 0x903A5A;
constexpr std::uint32_t kParty = 0x904062;            // the party's three character ids, then the reserve's
constexpr std::uint32_t kMemberRecord = 0x66972C;     // the character-to-record byte (MoveScript_EffectState in symbols.toml; docs/menu_lists.md section 8)

// The battle party's field objects (ObjTrio, 0x14C each): +0x89 the character id.
constexpr std::uint32_t kObjTrio = 0x802D40;
constexpr std::uint32_t kObjStride = 0x14C;
// The character records (CharacterRecords, 0xA4 each): +0xA the level, +0xC the EXP.
constexpr std::uint32_t kCharRecords = 0x903A70;
constexpr std::uint32_t kCharStride = 0xA4;
// Char_ExpTable: 99 rows of 8 bytes a roster index, the first word of a row the
// EXP its level takes.
constexpr std::uint32_t kExpTable = 0x658F48;

// The gene grid's cells: 0x939820 eighteen words zeroed on open, 0x939A60 the
// eighteen cells (a byte each, 0 empty); the battle task pool 0x93A000, 48 of 0x84.
constexpr std::uint32_t kGridWords = 0x939820;
constexpr std::uint32_t kGridCells = 0x939A60;
constexpr std::uint32_t kTasks = 0x93A000;
constexpr std::uint32_t kTaskStride = 0x84;
constexpr unsigned kTaskCount = 48;

// The state tables of handler 6's kinds 5..19 (read by the kinds' first call,
// unchecked in the original): each is named in symbols.toml; `run` is the
// run of code pointers that starts there (it runs on into the next tables) -
// past it the original would call data.
constexpr std::uint32_t kStatsStates = 0x66B044;      // kind 5
constexpr std::uint32_t kEquipStates = 0x66B060;      // kind 6
constexpr std::uint32_t kExpStates = 0x66B06C;        // kind 7
constexpr std::uint32_t kNextLevelStates = 0x66B078;  // kind 8
constexpr std::uint32_t kWideTitleStates = 0x66B084;  // kind 9
constexpr std::uint32_t kCursorBoxSizes = 0x66B090;   // kind 10's sizes, a word pair each
constexpr std::uint32_t kIconWheelStates = 0x66B0A0;  // kind 11
constexpr std::uint32_t kItemListStates = 0x66B0AC;   // kind 12
constexpr std::uint32_t kButtonRowStates = 0x66B0C0;  // kind 13
constexpr std::uint32_t kEquipCompareStates = 0x66B184;   // kind 16
constexpr std::uint32_t kAbilityStates = 0x66B198;    // kind 17
constexpr std::uint32_t kItemPanelStates = 0x66B1AC;  // kind 18
constexpr std::uint32_t kReserveStates = 0x66B1C0;    // kind 19
// The label list (kind 15): its piece lists, the text pointers and the rows
// (a count, then up to five text indices, 6 bytes a row).
constexpr std::uint32_t kLabelPieces = 0x66B0CC;
constexpr std::uint32_t kLabelRowPieces = 0x66B0F0;
constexpr std::uint32_t kLabelEndPieces = 0x66B0FC;
constexpr std::uint32_t kLabelTexts = 0x66B12C;
constexpr std::uint32_t kLabelRows = 0x66B158;

// DIV-0041's patch sites inside this group's bodies (widescreen.cpp kSlides):
// the imm32 of `mov ecx, imm32` or the imm16 of `cmp cx, imm16`, read back on
// every call.
constexpr std::uint32_t kLeftOff300Bound = 0x599CC6;  // -300 in MenuList_SlideLeftOff300
constexpr std::uint32_t kLeftOff100Bound = 0x599DF6;  // -100 in MenuList_SlideLeftOff100
constexpr std::uint32_t kLeftOff180Bound = 0x59A136;  // -180 in MenuList_SlideLeftOff180
constexpr std::uint32_t kLeftOff110Bound = 0x59A2B6;  // -110 in MenuList_SlideLeftOff110
constexpr std::uint32_t kGridOutRight = 0x598A08;     // 0x142 in GeneWin_GridSlideOut
constexpr std::uint32_t kGridOutLeft = 0x598A1C;      // -0xBE in GeneWin_GridSlideOut

}  // namespace at

// Callees of other groups, by address: ours since their owners merged, named by
// symbol, the values unchanged (round fourteen's rebinding, docs/round-14-cleanup.md).
constexpr std::uint32_t kRosterIndex = bof3::addr::CharId_ToRosterIndex;      // 0x4469D0, R3B's (the same value): CharId_ToRosterIndex (battle_result_callees.h), the byte at 0x66972C + id, 7 is 0
constexpr std::uint32_t kReserveList = bof3::addr::MenuList_ReserveWinDraw;      // R2H's: a panel's reserve list (menu_frame.cpp, DIV-0011's site 0x59AA98)

}  // namespace rest_2g
