// Group R4D's raw addresses (round fourteen, wave four; docs/rest_4d.md): the
// .data tables its dispatchers jump through (each a [[data]] entry in
// symbols.toml with the count its own reader's states reach, section 3 of the
// doc), the cells its code names by address, and the callees of other groups
// of this wave (R4C, R4E), called by address (ours since their owners merged,
// named by symbol, the values unchanged: round fourteen's rebinding,
// docs/round-14-cleanup.md).
#pragma once

#include <cstdint>

#include "bof3/symbols.gen.h"

namespace rest_4d {
namespace at {

// --- the state tables the group's dispatchers read (in address order) ---------
// CommuDraw (0x652A70[7], reached by R4B's 0x4572F0 on 0x9039F4 == 7):
// kDrawStates: now CommuDraw_States (symbols.toml). CommuDraw_States: 3, by 0x939A3E (CommuDraw_Dispatch)
// kDrawOpenSteps: now CommuDraw_OpenSteps (symbols.toml). CommuDraw_OpenSteps: 2, by 0x939A40 (state 0)
// kDrawShowSteps: now CommuDraw_ShowSteps (symbols.toml). CommuDraw_ShowSteps: 5, by 0x939A40 (state 1)
// CommuName (0x652A70[8], 0x9039F4 == 8):
// kNameStates: now CommuName_States (symbols.toml). CommuName_States: 5, by 0x939A3E (CommuName_Dispatch)
// kSlotSteps: now CommuName_SlotSteps (symbols.toml). CommuName_SlotSteps: 9, by 0x939A40 (state 1)
// kSlotRandomSteps: now CommuName_SlotRandomSteps (symbols.toml). CommuName_SlotRandomSteps: 3, by 0x939A3F (state 1, step 6)
// kSlotEntrySteps: now CommuName_SlotEntrySteps (symbols.toml). CommuName_SlotEntrySteps: 5, by 0x939A3F (state 1, step 7)
// kMemberSteps: now CommuName_MemberSteps (symbols.toml). CommuName_MemberSteps: 9, by 0x939A40 (state 2)
// kMemberRandomSteps: now CommuName_MemberRandomSteps (symbols.toml). CommuName_MemberRandomSteps: 3, by 0x939A3F (state 2, step 6)
// kMemberEntrySteps: now CommuName_MemberEntrySteps (symbols.toml). CommuName_MemberEntrySteps: 5, by 0x939A3F (state 2, step 7)
// The name pointers CommuName_MemberEntryOut reads by a record index: 7.
constexpr ::bof3::image::Va kRecordNames{0x669FA4};       // CommuName_RecordNames
constexpr unsigned kRecordNameCount = 7;

// --- the byte tables read in place (game data, never copied) -------------------
constexpr ::bof3::image::Va kDrawRows{0x652DEC};     // 8 rows of 9 category bytes, 9 the end
constexpr ::bof3::image::Va kDrawPools{0x652E34};    // 4 x (first message, count)
constexpr ::bof3::image::Va kDrawPicks{0x652E3C};    // 4 x (threshold, category, category or 0xFF)
constexpr ::bof3::image::Va kSlotDefaults{0x653200}; // 20 bytes a slot; the first five a name (R4A reads it too)
constexpr ::bof3::image::Va kPoolWords{0x803580};    // MessagePools: u16 offsets from 0x803580
constexpr ::bof3::image::Va kDrawWords{0x803780};    // the draw's message offsets (MessagePools + 0x200)
constexpr ::bof3::image::Va kTitleWords{0x803746};   // the title line's, by 0x675F8C (MessagePools + 0x1C6)
constexpr ::bof3::image::Va kBoardText{0x653084};    // CommuBoard's Font8 text
constexpr ::bof3::image::Va kBoardFmt3{0x653088};    // a format of three numbers
constexpr ::bof3::image::Va kBoardBlankA{0x653090};  // a format of no number
constexpr ::bof3::image::Va kBoardBlankB{0x653094};  // a format of no number
constexpr ::bof3::image::Va kNumberFmt{0x5E10C0};    // a format of one number (Area08_MessageFormat)

// --- the community's cells ------------------------------------------------------------
// kGame: now AreaW0c_TailState (symbols.toml). the community's game (R4B's 0x652A70 index)
// kPointer: now Rest4d_Pointer (symbols.toml). a record pointer: its +5 is a slot index
// kHow: now Rest4b_BoardFlag (symbols.toml). read once message 0xF2 is closed: 0 step 6, else step 7
// kAgain: now Rest4d_Again (symbols.toml). read once message 0xF3 / 0xF4 is closed (the two asks take it oppositely)
constexpr ::bof3::image::Va kState{0x939A3E};        // the game's state
// kStep2: now Rest4c_Step (symbols.toml). a step's own step
// kStep: now Rest4b_BoardStep (symbols.toml). the state's step
constexpr ::bof3::image::Va kCursor{0x675F8C};       // s8: a cursor (the draw's row)
// kYes: now Rest4c_Cursor (symbols.toml). s8: the yes / no cursor (CommuBoard: the lifted column)
// kKeptTrack: now Rest4d_KeptTrack (symbols.toml). the music track kept across the draw
// kHeader: now Rest4d_Header (symbols.toml). u16: the header's message (0xFFFF none)
// kEntryColumn: now Rest4d_EntryColumn (symbols.toml). the entry's column
constexpr ::bof3::image::Va kCount{0x675F95};        // a frame counter (the panels' slide, the reveal)
// kEntryDone: now Rest4d_EntryDone (symbols.toml). BareRetZero's answer
constexpr ::bof3::image::Va kName{0x675F98};         // five name bytes; the draw's picks, 10 a category
constexpr ::bof3::image::Va kPicked{0x675FC0};       // the draw's four counts
// kTitleA: now Rest4d_TitleA (symbols.toml). the title's two messages
// kTitleB: now Rest4d_TitleB (symbols.toml).
// kEntryA: now Rest4d_EntryA (symbols.toml). three bytes handed to BareRet
// kEntryB: now Rest4d_EntryB (symbols.toml).
// kEntryC: now Rest4d_EntryC (symbols.toml).
// kSlots: now Community_Records (symbols.toml). 60 cells of 8: in use when the first byte is not 0
constexpr std::uint32_t kSlotsEnd = 0x9048B0;
constexpr ::bof3::image::Va kSlotNames{0x9048F0};    // 5 bytes a slot
// kRecords: now CharacterRecords (symbols.toml). the character records, 0xA4 each
constexpr std::uint32_t kRecordStride = 0xA4;
constexpr ::bof3::image::Va kRecordFlags{0x903A7B};  // +0xB: bit 0 the record is in the party's list
constexpr std::uint32_t kRecordFlagsEnd = 0x903EF7;   // 7 records
// kColour: now Config_WindowColour (symbols.toml). the window style byte
// kPressed: now Input_Pressed (symbols.toml). Input_Pressed (u16)
constexpr ::bof3::image::Va kTextScratch{0x904BA0};  // the text scratch
constexpr ::bof3::image::Va kTextRecords{0x904CE0};  // Text_Records, 0x20 each
constexpr ::bof3::image::Va kTextRecordsEnd{0x904E00};   // nine

// --- callees of other groups of this wave, by address ------------------------------------
constexpr std::uint32_t kBoardCell = bof3::addr::Commu_DrawCard;    // R4C: a cell (x, y, value; 0xFF blank)
constexpr std::uint32_t kBoardPiece = bof3::addr::Commu_DrawPiece;   // R4C: a frame piece (x, y, piece)
constexpr std::uint32_t kSlotPanel = bof3::addr::CommuEntry_DrawPanel;    // R4E: a slot's panel (x, y, slot, flag)
constexpr std::uint32_t kListPiece = bof3::addr::Commu_DrawPiece6;    // R4E: a frame piece (x, y, piece)
constexpr std::uint32_t kSlotHand = bof3::addr::CommuCursor_DrawArrow;     // R4E: the list's hand (x, y, flag)
constexpr std::uint32_t kRandomName = bof3::addr::CommuName_MakeRandom;   // R4E: a name drawn at 0x675F98; answers its length + 1
constexpr std::uint32_t kMemberPanel = bof3::addr::CommuMember_DrawPanel;  // R4E: a member's panel (x, y, record, flag)
constexpr std::uint32_t kMemberPanels = bof3::addr::CommuMember_DrawAll; // R4E: the members' panels
constexpr std::uint32_t kMemberCount = bof3::addr::CommuMember_Count;  // R4E: the records in the list (al)
constexpr std::uint32_t kNthMember = bof3::addr::CommuMember_Nth;    // R4E: the n-th such record (eax, 0xFF none)
constexpr std::uint32_t kMemberHand = bof3::addr::CommuMember_DrawFrame;   // R4E: the members' hand (x, y, flag, 6)
constexpr std::uint32_t kEntryBox = bof3::addr::Commu_DrawTiledFrame;     // R4E: the entry's box (0x12, y, 0x24, 0x11)
constexpr std::uint32_t kMemberRename = bof3::addr::CommuName_CommitMember; // R4E: the member's new name written (a tail jump)
constexpr std::uint32_t kSlotRename = bof3::addr::CommuName_CommitEntry;   // R4E: the slot's new name written (a tail jump)

}  // namespace at
}  // namespace rest_4d
