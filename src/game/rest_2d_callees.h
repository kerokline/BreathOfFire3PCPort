// Group R2D's raw addresses (round fourteen, wave two; docs/rest_2d.md): the
// callees another group of this wave owns (called by address; ours since their
// owners merged, named by symbol, the values unchanged: round fourteen's
// rebinding, docs/round-14-cleanup.md), the .data tables the group's code reads,
// and the cells it names. The state tables its dispatchers jump through are
// named [[data]] entries in symbols.toml, with their readers' counts (section 3
// of the doc); their addresses are here for the code that reads them in place.
#pragma once

#include <cstdint>

#include "bof3/symbols.gen.h"

namespace rest_2d {
namespace at {

// --- callees of another group of this wave (by address) ----------------------------------
// R2C's (0x57F340..0x586980): the masters' screen's draws.
constexpr std::uint32_t kMemberPanel = bof3::addr::MasterPanel_DrawMember;   // (x, y, member, row): a member's stat panel
constexpr std::uint32_t kMemberLabel = bof3::addr::MasterPanel_DrawStats;   // (x, y, member): the panel's label box
constexpr std::uint32_t kPromptBox = bof3::addr::Menu_DrawPanelBox;     // (x, y, w, h, style byte): the prompt's box
// R2E's (0x58B1D0..0x58ED10): the Items screen's window set-ups.
constexpr std::uint32_t kItemsWindows = bof3::addr::FieldItems_InitWindows;  // (): the Items screen's windows placed
constexpr std::uint32_t kItemsReset = bof3::addr::FieldItems_CloseWindows;    // (): the Items screen's windows taken down

// --- the state tables (symbols.toml [[data]], counts by their readers) --------------------
// kAbilityEffects: now FieldAbility_Effects (symbols.toml). FieldAbility_Effects, 10: FieldAbility_Use's 0..9
// kStatusStates: now FieldMenuStatus_States (symbols.toml). FieldMenuStatus_States, 5, by 0x929F01
// kItemsStates: now FieldMenuItems_States (symbols.toml). FieldMenuItems_States, 11, by 0x929F01
// kItemsState5Steps: now FieldMenuItems_State5Steps (symbols.toml). FieldMenuItems_State5Steps, 3, by 0x929F02
// kQuitSteps: now MasterQuit_Steps (symbols.toml). MasterQuit_Steps, 9, by 0x9398D1
// kGrantSteps: now MasterGrant_Steps (symbols.toml). MasterGrant_Steps, 3, by 0x9398D1
constexpr unsigned kAbilityEffectCount = 10;
constexpr unsigned kStatusStateCount = 5;
constexpr unsigned kItemsStateCount = 11;
constexpr unsigned kItemsState5StepCount = 3;
constexpr unsigned kQuitStepCount = 9;
constexpr unsigned kGrantStepCount = 3;

// --- the masters' .data tables, read in place by the master byte (unbounded) ---------------
constexpr std::uint32_t kMasterMessages = 0x6644E8;  // u16 a master: its first message; + 4 .. + 0x11
constexpr std::uint32_t kMasterStats = 0x664370;     // 6 bytes a master: the stats copied to +0x89..+0x8E
constexpr std::uint32_t kMasterSkills = 0x6642A4;    // 6 pairs a master (12 bytes): level gained, ability
constexpr std::uint32_t kMasterLeavePose = 0x66459C; // a byte a master: the sprite's state on leaving, 0xFF none

// --- the masters' cells -------------------------------------------------------------------
// kMaster: now AreaW0c_TailSub (symbols.toml). the field mode tail's argument: the master
// kTailStep: now AreaW0c_TailState (symbols.toml). the field mode tail's step
constexpr std::uint32_t kScriptObject = 0x90384B;  // the chapters' counter byte: a sprite index here
// kSprites: now Sprite_Objects (symbols.toml). Sprite_Objects, 30 of 0xA4
// kCursor: now Rest2d_Cursor (symbols.toml). the member cursor
constexpr std::uint32_t kState = 0x9398CF;         // the screen's state (0x586670 jumps on it)
// kStep: now AreaW3f_AnswerMarkD1 (symbols.toml). the state's step
// kAnswer: now Rest2d_Answer (symbols.toml). the yes / no answer: 0 yes, 1 no

// --- the field menu's cells ----------------------------------------------------------------
// kMenuMode: now BattleE1_Mode (symbols.toml).
// kMenuState: now FieldS_State (symbols.toml).
// kMenuStep: now FieldS_Step (symbols.toml).
// kMenuSub: now Rest2d_MenuSub (symbols.toml).
constexpr std::uint32_t kMenuTimer = 0x929F04;
// kTopCursor: now MenuLists_Cursor (symbols.toml). the top bar's entry
// kStatusCursor: now BattleActorCopies_ItemActor (symbols.toml). the Status screen's member (s8)
// kMenuAnswer: now FieldS_Answer (symbols.toml).
// kItemsCategory: now Rest2d_ItemsCategory (symbols.toml). the Items screen's category cursor (s8, 0..3)
constexpr std::uint32_t kItemsPicks = 0x939898;    // a byte a category: the list's pick
constexpr std::uint32_t kItemsTops = 0x9398B8;     // a byte a category: the list's first row shown

// --- shared ---------------------------------------------------------------------------------
// kWindows: now WindowRecords (symbols.toml). WindowRecords, 22 of 0x24
constexpr std::uint32_t kWindowStride = 0x24;
constexpr unsigned kWindowCount = 22;
// kRecords: now CharacterRecords (symbols.toml). CharacterRecords, 8 of 0xA4
constexpr std::uint32_t kRecordStride = 0xA4;
constexpr std::uint32_t kWorking = 0x802DC0;       // ObjTrio + 0x80: the party's working records, 3 of 0x14C
constexpr std::uint32_t kObjStride = 0x14C;
// kStyle: now Config_WindowColour (symbols.toml). the window style byte
// kBackdrop: now Config_Background (symbols.toml). Menu_DrawBackdrop's kind
constexpr std::uint32_t kMessagePools = 0x803580;  // MessagePools: u16 offsets, then the text
// kPartyList: now Cond_Flags[0xd2] (symbols.toml). the party's ids
// kStoryFlags: now Cond_Flags[0xa0] (symbols.toml).
// kMasterBits: now Cond_Flags[0xd1] (symbols.toml). bits 3..5: masters 0xB, 0xD, 0xE's grants
// kAbilityBits: now Cond_Flags[0xf8] (symbols.toml). a bit an ability: taught by a master
constexpr std::uint32_t kAbilityRecords = 0x65C4C8;  // Ability_Records, 0x18 a record
constexpr std::uint32_t kItemIdLists = 0x656B00;   // Inventory_IdLists, 5 pointers
constexpr std::uint32_t kItemCountLists = 0x656B14;  // Inventory_CountLists, 5 pointers (the fifth 0)
constexpr std::uint32_t kItemFlags = 0x656B38;     // NameTable_Consumables + 0x10: a record's flags, 22 a record

}  // namespace at
}  // namespace rest_2d
