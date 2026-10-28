// Group R4B's raw addresses (round fourteen, wave four; docs/rest_4b.md): the
// .data tables its dispatchers jump through (each a [[data]] entry in
// symbols.toml with the count its own reader reaches, section 3 of the doc),
// the cells and constant tables its code names by address, and the callees of
// other groups of this wave, called by address (ours since their owners
// merged, named by symbol, the values unchanged: round fourteen's rebinding,
// docs/round-14-cleanup.md).
#pragma once

#include <cstdint>

#include "bof3/symbols.gen.h"

namespace rest_4b {
namespace at {

// --- the state tables the group's dispatchers read (one run of code words,
// read from several starting cells; each count runs to the end of its run) ---
// kTail14States: now CommuTail14_States (symbols.toml). CommuTail14_States: 11, by the s8 0x9039F4 (CommuTail14_Dispatch)
// kTail21States: now CommuTail21_States (symbols.toml). CommuTail21_States: 5 (= CommuTail14_States[6..10]), by 0x9039F4
// kTail22States: now CommuTail22_States (symbols.toml). CommuTail22_States: 2 (= CommuTail14_States[9..10]), by 0x9039F4
// kTail23States: now CommuTail23_States (symbols.toml). CommuTail23_States: 21, by 0x9039F4 (CommuTail23_Dispatch)
// kTailGames: now CommuTail_GameStates (symbols.toml). CommuTail_GameStates: 16 (= CommuTail23_States[5..20]), by the u8 0x9039F5
// kTail24States: now CommuTail24_States (symbols.toml). CommuTail24_States: 12 (= CommuTail23_States[9..20]), by 0x9039F4
// kTail25States: now CommuTail25_States (symbols.toml). CommuTail25_States: 7 (= CommuTail23_States[14..20]), by 0x9039F4
// kTail26States: now CommuTail26_States (symbols.toml). CommuTail26_States: 5, by 0x9039F4 (CommuTail26_Dispatch)
// kBoardStates: now CommuBoard_States (symbols.toml). CommuBoard_States: 14, by the u8 0x939A3E (CommuBoard_Dispatch)
// kBoardModes: now CommuBoard_ModeStates (symbols.toml). CommuBoard_ModeStates: 11 (= CommuBoard_States[3..13]), by the u8 0x675F86
// kBoardSteps: now CommuBoard_Steps (symbols.toml). CommuBoard_Steps: 8 (= CommuBoard_States[6..13]), by the u8 0x939A40
// kBoardStepsB: now CommuBoard_StepsB (symbols.toml). CommuBoard_StepsB: 6 (= CommuBoard_States[8..13]), by the u8 0x939A40

// --- the field tail's cells (Field_ModeTailKinds' kind, its state, its argument) ---
// kTailKind: now AreaW0a_TailKind (symbols.toml). Field_ModeTailKinds' index (s8)
// kTailState: now AreaW0c_TailState (symbols.toml). s8: the armed kind's state
// kTailArg: now AreaW0c_TailSub (symbols.toml). u8: the kind's argument (a record or building index)
// kTailObject: now Rest4d_Pointer (symbols.toml). the field object the arming handler kept (its +0x85, +8, +0x88)

// --- the board's cells -------------------------------------------------------------
// kBoardFlag: now Rest4b_BoardFlag (symbols.toml). picks the stream (8 when 0, else 0xA)
constexpr ::bof3::image::Va kBoardTrack{0x939A3D};     // the music track kept across the stream
// kBoardState: now Rest4b_BoardState (symbols.toml). CommuBoard_States' index
// kBoardStep: now Rest4b_BoardStep (symbols.toml). CommuBoard_Steps' / _StepsB' index
// kCursorMode: now Rest4b_CursorMode (symbols.toml). the grid cursor's mode: 0, 1, 2
// kCursorA: now Rest4b_CursorA (symbols.toml).
// kCursorB: now Rest4b_CursorB (symbols.toml).
// kKeptHelp: now Rest4b_KeptHelp (symbols.toml). u16: the help line kept by R4C's 0x459EE0
// kHelp: now Rest4b_Help (symbols.toml). u16: the help line's message (0xFFFF none)
// kToggle: now Rest4b_Toggle (symbols.toml). a 0 / 1 toggle (the yes / no row)
// kPickC: now Rest4b_PickC (symbols.toml).
// kPickB: now Rest4b_PickB (symbols.toml).
constexpr ::bof3::image::Va kPick{0x675F7F};           // s8: the picked slot
// kListCursor: now Rest4b_ListCursor (symbols.toml). s8: the list cursor
// kKeptStep: now Rest4b_KeptStep (symbols.toml). the step kept by R4C's 0x459EE0
// kPickE: now Rest4b_PickE (symbols.toml).
// kPickD: now Rest4b_PickD (symbols.toml).
// kSavedB: now Rest4b_SavedB (symbols.toml). the grid cursor kept on confirm (0x675F76, 0x675F75, 0x675F74)
// kSavedA: now Rest4b_SavedA (symbols.toml).
// kSavedMode: now Rest4b_SavedMode (symbols.toml).

// --- the community records (the save block) ------------------------------------------
// kCountA: now Rest4b_CountA (symbols.toml). s8, printed by the panel
constexpr ::bof3::image::Va kListCount{0x9046CB};      // the lists' length
// kCountC: now Rest4a_LevelLit (symbols.toml). printed by the panel
constexpr ::bof3::image::Va kMembers{0x9046D0};        // 60 records of 8: +0 used, +1 slot, +2 item, +3 two nibbles, +4 a time
constexpr std::uint32_t kMemberStride = 8;
constexpr std::uint32_t kMembersEnd = 0x9048B0;
constexpr ::bof3::image::Va kSlots{0x9048A8};          // by a slot 1..8: 0x9048B0 + 8 * (slot - 1); +0 kind, +1, +2, +3, +4 a time
constexpr ::bof3::image::Va kNames{0x9048F0};          // 5 bytes a record
// kClock: now Game_BattleCount (symbols.toml). u32: the time the records' +4 are taken from
// kSoundBankBits: now AreaW0b_PartySet (symbols.toml). bit 7 set, the low seven pick the sound bank
// kMusicTrack: now Music_Track (symbols.toml). Music_Track

// --- cells read by the game's other code -----------------------------------------------
// kRequest: now Field_Request (symbols.toml). Field_Request
constexpr ::bof3::image::Va kMessageIndex{0x7DEE48};   // MsgBoxState's message index (u16)
// kScriptFlags: now Rest4a_RemovedCount (symbols.toml). Field_ScriptFlags
constexpr ::bof3::image::Va kScriptFlags2{0x905BA4};   // Field_ScriptFlags2 (u16)
// kBusyA: now Rest4b_BusyA (symbols.toml).
// kBusyB: now Rest4b_BusyB (symbols.toml).
constexpr ::bof3::image::Va kSpriteCurrent{0x937F88};  // Sprite_Current
// kClutDirty: now Gfx_ClutStripDirty (symbols.toml). Gfx_ClutStripDirty
constexpr ::bof3::image::Va kFrame{0x937F94};          // Frame_Counter
// kPacketNext: now Gfx_PacketNext (symbols.toml). Gfx_PacketNext
// kPressed: now Input_Pressed (symbols.toml). Input_Pressed (u16)
// kConfirm: now Field_ConfirmButtons (symbols.toml). Field_ConfirmButtons (u16)
// kCancel: now Field_CancelButtons (symbols.toml). Field_CancelButtons (u16)
// kColour: now Config_WindowColour (symbols.toml). the window style byte
// kArea: now Game_AreaNumber (symbols.toml). Game_AreaNumber (u16)
constexpr ::bof3::image::Va kTextRecords{0x904CE0};    // Text_Records' first 16 bytes
constexpr ::bof3::image::Va kTextBuffer{0x904BA0};     // the text scratch sprintf writes
constexpr ::bof3::image::Va kMessagePools{0x803580};   // MessagePools

// --- constant tables the code reads in place (indexes as the code computes them) -------
constexpr ::bof3::image::Va kGiftItems{0x652A10};      // 48 pairs (item, category) by tier and a draw
constexpr ::bof3::image::Va kGiftLines{0x652AC4};      // 6 bytes by a nibble: message, message, count
constexpr ::bof3::image::Va kPickHelp{0x652AF0};       // u16 by a slot's kind
constexpr ::bof3::image::Va kPickHelpB{0x652AF8};      // u16 by 0x675F7F
constexpr ::bof3::image::Va kPickNext{0x652B44};       // bytes by 0x675F7F
constexpr ::bof3::image::Va kPickNextHelp{0x652B50};   // u16 by 0x675F83
constexpr ::bof3::image::Va kPickPairs{0x652B58};      // two bytes by 0x675F83: a count, a next
constexpr ::bof3::image::Va kBarHighlight{0x652B60};   // bytes by a slot's kind
constexpr ::bof3::image::Va kSlotIcons{0x652B6C};      // bytes by a slot's kind
constexpr ::bof3::image::Va kSlotLines{0x652B7C};      // 12 bytes a slot: six s16
constexpr ::bof3::image::Va kDigitCells{0x652C0C};     // bytes by a number
constexpr ::bof3::image::Va kSprites{0x652C18};        // 6 bytes a sprite: u, v, w, h, clut x, clut y
constexpr ::bof3::image::Va kLists{0x652C6C};          // two bytes a list: first, count
constexpr ::bof3::image::Va kSlotHelp{0x652C72};       // u16 by a slot past 8
constexpr ::bof3::image::Va kSlotOffsets{0x652C74};    // 4 bytes: s16 x, s16 y
constexpr ::bof3::image::Va kBars{0x653210};           // 20 bytes a record id: four bar lengths first
constexpr ::bof3::image::Va kAreaLimits{0x653601};     // a byte by Game_AreaNumber
constexpr ::bof3::image::Va kPanelLabel{0x669E10};     // the panel's label text
constexpr ::bof3::image::Va kListTextA{0x669E68};      // pointers: the record list's lines
constexpr ::bof3::image::Va kListTexts{0x669EE0};      // pointers: the lists' lines
// kCountFormat: now Boss26Fx_CountFormat (symbols.toml). Boss26Fx_CountFormat
constexpr ::bof3::image::Va kNumberFormat{0x5E10C0};   // Area08_MessageFormat

// --- other groups' functions of this wave, by address --------------------------------
constexpr ::bof3::image::CodeVa kR4ACountSlots{bof3::addr::CommuSim_SumKindsAB};  // R4A: void(void)
constexpr ::bof3::image::CodeVa kR4ASettle{bof3::addr::CommuSim_TickKind5};      // R4A: void(void)
constexpr ::bof3::image::CodeVa kR4CAsk{bof3::addr::Commu_PushSubscreen};         // R4C: void(void) - the yes / no row armed
constexpr ::bof3::image::CodeVa kR4EHand{bof3::addr::CommuCursor_DrawArrow};        // R4E: void(short x, short y, byte flash) - a cursor tile

}  // namespace at
}  // namespace rest_4b
