// Group R4B's raw addresses (round fourteen, wave four; docs/rest_4b.md): the
// .data tables its dispatchers jump through (each a [[data]] entry in
// symbols.toml with the count its own reader reaches, section 3 of the doc),
// the cells and constant tables its code names by address, and the callees of
// other groups of this wave, called by address until the round's rebinding.
#pragma once

#include <cstdint>

namespace rest_4b {
namespace at {

// --- the state tables the group's dispatchers read (one run of code words,
// read from several starting cells; each count runs to the end of its run) ---
constexpr std::uint32_t kTail14States = 0x6529E4;   // CommuTail14_States: 11, by the s8 0x9039F4 (CommuTail14_Dispatch)
constexpr std::uint32_t kTail21States = 0x6529FC;   // CommuTail21_States: 5 (= CommuTail14_States[6..10]), by 0x9039F4
constexpr std::uint32_t kTail22States = 0x652A08;   // CommuTail22_States: 2 (= CommuTail14_States[9..10]), by 0x9039F4
constexpr std::uint32_t kTail23States = 0x652A70;   // CommuTail23_States: 21, by 0x9039F4 (CommuTail23_Dispatch)
constexpr std::uint32_t kTailGames = 0x652A84;      // CommuTail_GameStates: 16 (= CommuTail23_States[5..20]), by the u8 0x9039F5
constexpr std::uint32_t kTail24States = 0x652A94;   // CommuTail24_States: 12 (= CommuTail23_States[9..20]), by 0x9039F4
constexpr std::uint32_t kTail25States = 0x652AA8;   // CommuTail25_States: 7 (= CommuTail23_States[14..20]), by 0x9039F4
constexpr std::uint32_t kTail26States = 0x652AE4;   // CommuTail26_States: 5, by 0x9039F4 (CommuTail26_Dispatch)
constexpr std::uint32_t kBoardStates = 0x652B0C;    // CommuBoard_States: 14, by the u8 0x939A3E (CommuBoard_Dispatch)
constexpr std::uint32_t kBoardModes = 0x652B18;     // CommuBoard_ModeStates: 11 (= CommuBoard_States[3..13]), by the u8 0x675F86
constexpr std::uint32_t kBoardSteps = 0x652B24;     // CommuBoard_Steps: 8 (= CommuBoard_States[6..13]), by the u8 0x939A40
constexpr std::uint32_t kBoardStepsB = 0x652B2C;    // CommuBoard_StepsB: 6 (= CommuBoard_States[8..13]), by the u8 0x939A40

// --- the field tail's cells (Field_ModeTailKinds' kind, its state, its argument) ---
constexpr std::uint32_t kTailKind = 0x9039F3;       // Field_ModeTailKinds' index (s8)
constexpr std::uint32_t kTailState = 0x9039F4;      // s8: the armed kind's state
constexpr std::uint32_t kTailArg = 0x9039F5;        // u8: the kind's argument (a record or building index)
constexpr std::uint32_t kTailObject = 0x939A38;     // the field object the arming handler kept (its +0x85, +8, +0x88)

// --- the board's cells -------------------------------------------------------------
constexpr std::uint32_t kBoardFlag = 0x939A3C;      // picks the stream (8 when 0, else 0xA)
constexpr std::uint32_t kBoardTrack = 0x939A3D;     // the music track kept across the stream
constexpr std::uint32_t kBoardState = 0x939A3E;     // CommuBoard_States' index
constexpr std::uint32_t kBoardStep = 0x939A40;      // CommuBoard_Steps' / _StepsB' index
constexpr std::uint32_t kCursorMode = 0x675F74;     // the grid cursor's mode: 0, 1, 2
constexpr std::uint32_t kCursorA = 0x675F75;
constexpr std::uint32_t kCursorB = 0x675F76;
constexpr std::uint32_t kKeptHelp = 0x675F78;       // u16: the help line kept by R4C's 0x459EE0
constexpr std::uint32_t kHelp = 0x675F7A;           // u16: the help line's message (0xFFFF none)
constexpr std::uint32_t kToggle = 0x675F7C;         // a 0 / 1 toggle (the yes / no row)
constexpr std::uint32_t kPickC = 0x675F7D;
constexpr std::uint32_t kPickB = 0x675F7E;
constexpr std::uint32_t kPick = 0x675F7F;           // s8: the picked slot
constexpr std::uint32_t kListCursor = 0x675F80;     // s8: the list cursor
constexpr std::uint32_t kKeptStep = 0x675F81;       // the step kept by R4C's 0x459EE0
constexpr std::uint32_t kPickE = 0x675F82;
constexpr std::uint32_t kPickD = 0x675F83;
constexpr std::uint32_t kSavedB = 0x675F84;         // the grid cursor kept on confirm (0x675F76, 0x675F75, 0x675F74)
constexpr std::uint32_t kSavedA = 0x675F85;
constexpr std::uint32_t kSavedMode = 0x675F86;

// --- the community records (the save block) ------------------------------------------
constexpr std::uint32_t kCountA = 0x9046CA;         // s8, printed by the panel
constexpr std::uint32_t kListCount = 0x9046CB;      // the lists' length
constexpr std::uint32_t kCountC = 0x9046CC;         // printed by the panel
constexpr std::uint32_t kMembers = 0x9046D0;        // 60 records of 8: +0 used, +1 slot, +2 item, +3 two nibbles, +4 a time
constexpr std::uint32_t kMemberStride = 8;
constexpr std::uint32_t kMembersEnd = 0x9048B0;
constexpr std::uint32_t kSlots = 0x9048A8;          // by a slot 1..8: 0x9048B0 + 8 * (slot - 1); +0 kind, +1, +2, +3, +4 a time
constexpr std::uint32_t kNames = 0x9048F0;          // 5 bytes a record
constexpr std::uint32_t kClock = 0x904134;          // u32: the time the records' +4 are taken from
constexpr std::uint32_t kSoundBankBits = 0x90412C;  // bit 7 set, the low seven pick the sound bank
constexpr std::uint32_t kMusicTrack = 0x904131;     // Music_Track

// --- cells read by the game's other code -----------------------------------------------
constexpr std::uint32_t kRequest = 0x66C7D8;        // Field_Request
constexpr std::uint32_t kMessageIndex = 0x7DEE48;   // MsgBoxState's message index (u16)
constexpr std::uint32_t kScriptFlags = 0x9039A0;    // Field_ScriptFlags
constexpr std::uint32_t kScriptFlags2 = 0x905BA4;   // Field_ScriptFlags2 (u16)
constexpr std::uint32_t kBusyA = 0x904A90;
constexpr std::uint32_t kBusyB = 0x937F80;
constexpr std::uint32_t kSpriteCurrent = 0x937F88;  // Sprite_Current
constexpr std::uint32_t kClutDirty = 0x937F90;      // Gfx_ClutStripDirty
constexpr std::uint32_t kFrame = 0x937F94;          // Frame_Counter
constexpr std::uint32_t kPacketNext = 0x7E0670;     // Gfx_PacketNext
constexpr std::uint32_t kPressed = 0x7E1BEC;        // Input_Pressed (u16)
constexpr std::uint32_t kConfirm = 0x90358E;        // Field_ConfirmButtons (u16)
constexpr std::uint32_t kCancel = 0x903590;         // Field_CancelButtons (u16)
constexpr std::uint32_t kColour = 0x903A5A;         // the window style byte
constexpr std::uint32_t kArea = 0x904EFC;           // Game_AreaNumber (u16)
constexpr std::uint32_t kTextRecords = 0x904CE0;    // Text_Records' first 16 bytes
constexpr std::uint32_t kTextBuffer = 0x904BA0;     // the text scratch sprintf writes
constexpr std::uint32_t kMessagePools = 0x803580;   // MessagePools

// --- constant tables the code reads in place (indexes as the code computes them) -------
constexpr std::uint32_t kGiftItems = 0x652A10;      // 48 pairs (item, category) by tier and a draw
constexpr std::uint32_t kGiftLines = 0x652AC4;      // 6 bytes by a nibble: message, message, count
constexpr std::uint32_t kPickHelp = 0x652AF0;       // u16 by a slot's kind
constexpr std::uint32_t kPickHelpB = 0x652AF8;      // u16 by 0x675F7F
constexpr std::uint32_t kPickNext = 0x652B44;       // bytes by 0x675F7F
constexpr std::uint32_t kPickNextHelp = 0x652B50;   // u16 by 0x675F83
constexpr std::uint32_t kPickPairs = 0x652B58;      // two bytes by 0x675F83: a count, a next
constexpr std::uint32_t kBarHighlight = 0x652B60;   // bytes by a slot's kind
constexpr std::uint32_t kSlotIcons = 0x652B6C;      // bytes by a slot's kind
constexpr std::uint32_t kSlotLines = 0x652B7C;      // 12 bytes a slot: six s16
constexpr std::uint32_t kDigitCells = 0x652C0C;     // bytes by a number
constexpr std::uint32_t kSprites = 0x652C18;        // 6 bytes a sprite: u, v, w, h, clut x, clut y
constexpr std::uint32_t kLists = 0x652C6C;          // two bytes a list: first, count
constexpr std::uint32_t kSlotHelp = 0x652C72;       // u16 by a slot past 8
constexpr std::uint32_t kSlotOffsets = 0x652C74;    // 4 bytes: s16 x, s16 y
constexpr std::uint32_t kBars = 0x653210;           // 20 bytes a record id: four bar lengths first
constexpr std::uint32_t kAreaLimits = 0x653601;     // a byte by Game_AreaNumber
constexpr std::uint32_t kPanelLabel = 0x669E10;     // the panel's label text
constexpr std::uint32_t kListTextA = 0x669E68;      // pointers: the record list's lines
constexpr std::uint32_t kListTexts = 0x669EE0;      // pointers: the lists' lines
constexpr std::uint32_t kCountFormat = 0x64D3EC;    // Boss26Fx_CountFormat
constexpr std::uint32_t kNumberFormat = 0x5E10C0;   // Area08_MessageFormat

// --- other groups' functions of this wave, by address until the round's rebinding ----
constexpr std::uint32_t kR4ACountSlots = 0x456080;  // R4A: void(void)
constexpr std::uint32_t kR4ASettle = 0x455950;      // R4A: void(void)
constexpr std::uint32_t kR4CAsk = 0x459EE0;         // R4C: void(void) - the yes / no row armed
constexpr std::uint32_t kR4EHand = 0x45ECC0;        // R4E: void(short x, short y, byte flash) - a cursor tile

}  // namespace at
}  // namespace rest_4b
