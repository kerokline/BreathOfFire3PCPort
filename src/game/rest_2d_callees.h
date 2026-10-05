// Group R2D's raw addresses (round fourteen, wave two; docs/rest_2d.md): the
// callees another group of this wave owns (called by address until it merges,
// the round's rebinding names them), the .data tables the group's code reads,
// and the cells it names. The state tables its dispatchers jump through are
// named [[data]] entries in symbols.toml, with their readers' counts (section 3
// of the doc); their addresses are here for the code that reads them in place.
#pragma once

#include <cstdint>

namespace rest_2d {
namespace at {

// --- callees of another group of this wave (raw until the rebinding) ---------------------
// R2C's (0x57F340..0x586980): the masters' screen's draws.
constexpr std::uint32_t kMemberPanel = 0x585DC0;   // (x, y, member, row): a member's stat panel
constexpr std::uint32_t kMemberLabel = 0x585BE0;   // (x, y, member): the panel's label box
constexpr std::uint32_t kPromptBox = 0x586160;     // (x, y, w, h, style byte): the prompt's box
// R2E's (0x58B1D0..0x58ED10): the Items screen's window set-ups.
constexpr std::uint32_t kItemsWindows = 0x58BC30;  // (): the Items screen's windows placed
constexpr std::uint32_t kItemsReset = 0x58C2A0;    // (): the Items screen's windows taken down

// --- the state tables (symbols.toml [[data]], counts by their readers) --------------------
constexpr std::uint32_t kAbilityEffects = 0x6672EC;    // FieldAbility_Effects, 10: FieldAbility_Use's 0..9
constexpr std::uint32_t kStatusStates = 0x667314;      // FieldMenuStatus_States, 5, by 0x929F01
constexpr std::uint32_t kItemsStates = 0x667328;       // FieldMenuItems_States, 11, by 0x929F01
constexpr std::uint32_t kItemsState5Steps = 0x667354;  // FieldMenuItems_State5Steps, 3, by 0x929F02
constexpr std::uint32_t kQuitSteps = 0x66456C;         // MasterQuit_Steps, 9, by 0x9398D1
constexpr std::uint32_t kGrantSteps = 0x664590;        // MasterGrant_Steps, 3, by 0x9398D1
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
constexpr std::uint32_t kMaster = 0x9039F5;        // the field mode tail's argument: the master
constexpr std::uint32_t kTailStep = 0x9039F4;      // the field mode tail's step
constexpr std::uint32_t kScriptObject = 0x90384B;  // the chapters' counter byte: a sprite index here
constexpr std::uint32_t kSprites = 0x7DEE80;       // Sprite_Objects, 30 of 0xA4
constexpr std::uint32_t kCursor = 0x9398CE;        // the member cursor
constexpr std::uint32_t kState = 0x9398CF;         // the screen's state (0x586670 jumps on it)
constexpr std::uint32_t kStep = 0x9398D1;          // the state's step
constexpr std::uint32_t kAnswer = 0x9398D2;        // the yes / no answer: 0 yes, 1 no

// --- the field menu's cells ----------------------------------------------------------------
constexpr std::uint32_t kMenuMode = 0x929F00;
constexpr std::uint32_t kMenuState = 0x929F01;
constexpr std::uint32_t kMenuStep = 0x929F02;
constexpr std::uint32_t kMenuSub = 0x929F03;
constexpr std::uint32_t kMenuTimer = 0x929F04;
constexpr std::uint32_t kTopCursor = 0x929F05;     // the top bar's entry
constexpr std::uint32_t kStatusCursor = 0x929F06;  // the Status screen's member (s8)
constexpr std::uint32_t kMenuAnswer = 0x929F0B;
constexpr std::uint32_t kItemsCategory = 0x6BDFA0; // the Items screen's category cursor (s8, 0..3)
constexpr std::uint32_t kItemsPicks = 0x939898;    // a byte a category: the list's pick
constexpr std::uint32_t kItemsTops = 0x9398B8;     // a byte a category: the list's first row shown

// --- shared ---------------------------------------------------------------------------------
constexpr std::uint32_t kWindows = 0x803160;       // WindowRecords, 22 of 0x24
constexpr std::uint32_t kWindowStride = 0x24;
constexpr unsigned kWindowCount = 22;
constexpr std::uint32_t kRecords = 0x903A70;       // CharacterRecords, 8 of 0xA4
constexpr std::uint32_t kRecordStride = 0xA4;
constexpr std::uint32_t kWorking = 0x802DC0;       // ObjTrio + 0x80: the party's working records, 3 of 0x14C
constexpr std::uint32_t kObjStride = 0x14C;
constexpr std::uint32_t kStyle = 0x903A5A;         // the window style byte
constexpr std::uint32_t kBackdrop = 0x903A5B;      // Menu_DrawBackdrop's kind
constexpr std::uint32_t kMessagePools = 0x803580;  // MessagePools: u16 offsets, then the text
constexpr std::uint32_t kPartyList = 0x904062;     // the party's ids
constexpr std::uint32_t kStoryFlags = 0x904030;
constexpr std::uint32_t kMasterBits = 0x904061;    // bits 3..5: masters 0xB, 0xD, 0xE's grants
constexpr std::uint32_t kAbilityBits = 0x904088;   // a bit an ability: taught by a master
constexpr std::uint32_t kAbilityRecords = 0x65C4C8;  // Ability_Records, 0x18 a record
constexpr std::uint32_t kItemIdLists = 0x656B00;   // Inventory_IdLists, 5 pointers
constexpr std::uint32_t kItemCountLists = 0x656B14;  // Inventory_CountLists, 5 pointers (the fifth 0)
constexpr std::uint32_t kItemFlags = 0x656B38;     // NameTable_Consumables + 0x10: a record's flags, 22 a record

}  // namespace at
}  // namespace rest_2d
