// Group R4D's raw addresses (round fourteen, wave four; docs/rest_4d.md): the
// .data tables its dispatchers jump through (each a [[data]] entry in
// symbols.toml with the count its own reader's states reach, section 3 of the
// doc), the cells its code names by address, and the callees of other groups
// of this wave (R4C, R4E), called by address until the round's rebinding.
#pragma once

#include <cstdint>

namespace rest_4d {
namespace at {

// --- the state tables the group's dispatchers read (in address order) ---------
// CommuDraw (0x652A70[7], reached by R4B's 0x4572F0 on 0x9039F4 == 7):
constexpr std::uint32_t kDrawStates = 0x652DC4;        // CommuDraw_States: 3, by 0x939A3E (CommuDraw_Dispatch)
constexpr std::uint32_t kDrawOpenSteps = 0x652DD0;     // CommuDraw_OpenSteps: 2, by 0x939A40 (state 0)
constexpr std::uint32_t kDrawShowSteps = 0x652DD8;     // CommuDraw_ShowSteps: 5, by 0x939A40 (state 1)
// CommuName (0x652A70[8], 0x9039F4 == 8):
constexpr std::uint32_t kNameStates = 0x652E48;        // CommuName_States: 5, by 0x939A3E (CommuName_Dispatch)
constexpr std::uint32_t kSlotSteps = 0x652E5C;         // CommuName_SlotSteps: 9, by 0x939A40 (state 1)
constexpr std::uint32_t kSlotRandomSteps = 0x652E80;   // CommuName_SlotRandomSteps: 3, by 0x939A3F (state 1, step 6)
constexpr std::uint32_t kSlotEntrySteps = 0x652E8C;    // CommuName_SlotEntrySteps: 5, by 0x939A3F (state 1, step 7)
constexpr std::uint32_t kMemberSteps = 0x652EA0;       // CommuName_MemberSteps: 9, by 0x939A40 (state 2)
constexpr std::uint32_t kMemberRandomSteps = 0x652EC4; // CommuName_MemberRandomSteps: 3, by 0x939A3F (state 2, step 6)
constexpr std::uint32_t kMemberEntrySteps = 0x652ED0;  // CommuName_MemberEntrySteps: 5, by 0x939A3F (state 2, step 7)
// The name pointers CommuName_MemberEntryOut reads by a record index: 7.
constexpr std::uint32_t kRecordNames = 0x669FA4;       // CommuName_RecordNames
constexpr unsigned kRecordNameCount = 7;

// --- the byte tables read in place (game data, never copied) -------------------
constexpr std::uint32_t kDrawRows = 0x652DEC;     // 8 rows of 9 category bytes, 9 the end
constexpr std::uint32_t kDrawPools = 0x652E34;    // 4 x (first message, count)
constexpr std::uint32_t kDrawPicks = 0x652E3C;    // 4 x (threshold, category, category or 0xFF)
constexpr std::uint32_t kSlotDefaults = 0x653200; // 20 bytes a slot; the first five a name (R4A reads it too)
constexpr std::uint32_t kPoolWords = 0x803580;    // MessagePools: u16 offsets from 0x803580
constexpr std::uint32_t kDrawWords = 0x803780;    // the draw's message offsets (MessagePools + 0x200)
constexpr std::uint32_t kTitleWords = 0x803746;   // the title line's, by 0x675F8C (MessagePools + 0x1C6)
constexpr std::uint32_t kBoardText = 0x653084;    // CommuBoard's Font8 text
constexpr std::uint32_t kBoardFmt3 = 0x653088;    // a format of three numbers
constexpr std::uint32_t kBoardBlankA = 0x653090;  // a format of no number
constexpr std::uint32_t kBoardBlankB = 0x653094;  // a format of no number
constexpr std::uint32_t kNumberFmt = 0x5E10C0;    // a format of one number (Area08_MessageFormat)

// --- the community's cells ------------------------------------------------------------
constexpr std::uint32_t kGame = 0x9039F4;         // the community's game (R4B's 0x652A70 index)
constexpr std::uint32_t kPointer = 0x939A38;      // a record pointer: its +5 is a slot index
constexpr std::uint32_t kHow = 0x939A3C;          // read once message 0xF2 is closed: 0 step 6, else step 7
constexpr std::uint32_t kAgain = 0x939A3D;        // read once message 0xF3 / 0xF4 is closed (the two asks take it oppositely)
constexpr std::uint32_t kState = 0x939A3E;        // the game's state
constexpr std::uint32_t kStep2 = 0x939A3F;        // a step's own step
constexpr std::uint32_t kStep = 0x939A40;         // the state's step
constexpr std::uint32_t kCursor = 0x675F8C;       // s8: a cursor (the draw's row)
constexpr std::uint32_t kYes = 0x675F8D;          // s8: the yes / no cursor (CommuBoard: the lifted column)
constexpr std::uint32_t kKeptTrack = 0x675F90;    // the music track kept across the draw
constexpr std::uint32_t kHeader = 0x675F92;       // u16: the header's message (0xFFFF none)
constexpr std::uint32_t kEntryColumn = 0x675F94;  // the entry's column
constexpr std::uint32_t kCount = 0x675F95;        // a frame counter (the panels' slide, the reveal)
constexpr std::uint32_t kEntryDone = 0x675F96;    // BareRetZero's answer
constexpr std::uint32_t kName = 0x675F98;         // five name bytes; the draw's picks, 10 a category
constexpr std::uint32_t kPicked = 0x675FC0;       // the draw's four counts
constexpr std::uint32_t kTitleA = 0x675FC5;       // the title's two messages
constexpr std::uint32_t kTitleB = 0x675FC6;
constexpr std::uint32_t kEntryA = 0x675FCA;       // three bytes handed to BareRet
constexpr std::uint32_t kEntryB = 0x675FCB;
constexpr std::uint32_t kEntryC = 0x675FCC;
constexpr std::uint32_t kSlots = 0x9046D0;        // 60 cells of 8: in use when the first byte is not 0
constexpr std::uint32_t kSlotsEnd = 0x9048B0;
constexpr std::uint32_t kSlotNames = 0x9048F0;    // 5 bytes a slot
constexpr std::uint32_t kRecords = 0x903A70;      // the character records, 0xA4 each
constexpr std::uint32_t kRecordStride = 0xA4;
constexpr std::uint32_t kRecordFlags = 0x903A7B;  // +0xB: bit 0 the record is in the party's list
constexpr std::uint32_t kRecordFlagsEnd = 0x903EF7;   // 7 records
constexpr std::uint32_t kColour = 0x903A5A;       // the window style byte
constexpr std::uint32_t kPressed = 0x7E1BEC;      // Input_Pressed (u16)
constexpr std::uint32_t kTextScratch = 0x904BA0;  // the text scratch
constexpr std::uint32_t kTextRecords = 0x904CE0;  // Text_Records, 0x20 each
constexpr std::uint32_t kTextRecordsEnd = 0x904E00;   // nine

// --- callees of other groups of this wave, by address ------------------------------------
constexpr std::uint32_t kBoardCell = 0x45B2C0;    // R4C: a cell (x, y, value; 0xFF blank)
constexpr std::uint32_t kBoardPiece = 0x45B400;   // R4C: a frame piece (x, y, piece)
constexpr std::uint32_t kSlotPanel = 0x45E870;    // R4E: a slot's panel (x, y, slot, flag)
constexpr std::uint32_t kListPiece = 0x45EC00;    // R4E: a frame piece (x, y, piece)
constexpr std::uint32_t kSlotHand = 0x45ECC0;     // R4E: the list's hand (x, y, flag)
constexpr std::uint32_t kRandomName = 0x45ED70;   // R4E: a name drawn at 0x675F98; answers its length + 1
constexpr std::uint32_t kMemberPanel = 0x45EE10;  // R4E: a member's panel (x, y, record, flag)
constexpr std::uint32_t kMemberPanels = 0x45EF90; // R4E: the members' panels
constexpr std::uint32_t kMemberCount = 0x45F000;  // R4E: the records in the list (al)
constexpr std::uint32_t kNthMember = 0x45F020;    // R4E: the n-th such record (eax, 0xFF none)
constexpr std::uint32_t kMemberHand = 0x45F050;   // R4E: the members' hand (x, y, flag, 6)
constexpr std::uint32_t kEntryBox = 0x45F1A0;     // R4E: the entry's box (0x12, y, 0x24, 0x11)
constexpr std::uint32_t kMemberRename = 0x45F5A0; // R4E: the member's new name written (a tail jump)
constexpr std::uint32_t kSlotRename = 0x45F650;   // R4E: the slot's new name written (a tail jump)

}  // namespace at
}  // namespace rest_4d
