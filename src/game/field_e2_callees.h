// The raw addresses field_e2.cpp reads or calls that symbols.toml does not
// name - each a load-bearing constant (CLAUDE.md rule 3). docs/field_e2.md.
#pragma once

#include <cstdint>

#include "bof3/symbols.gen.h"
#include "game/rdata_consts.h"

namespace field_e2::at {

// --- callees nobody owns, called through the harness by address (SH_AT) ------
constexpr std::uint32_t kHpLoss2 = bof3::addr::Char_LoseAp;   // R2A: (amount word, member byte): record +0x1A down, never below 1 (Char_LoseHp's AP twin)
constexpr std::uint32_t kFtol = 0x5B9550;            // the CRT's _ftol - st(0) truncated to edx:eax (inlined here, x87 as the CRT)
constexpr std::uint32_t kLoadWait = bof3::addr::MasterTalk_Reset;        // no arguments, no calls (FieldTail_LoadBank's step 1)
constexpr std::uint32_t kLoadStep = bof3::addr::MasterTalk_Dispatch;        // jmp through 0x66450C by the byte 0x9398CF (FieldTail_LoadBank's step 2, a tail jump)
constexpr std::uint32_t kTradeBox = ::bof3::addr::Panel_DrawWindow;   // 0x469750 (E1B's): five words: a window drawn (the trade screen's frame)
constexpr std::uint32_t kTradeList = bof3::addr::ItemTrade_DrawList;       // (flag): the trade list drawn
constexpr std::uint32_t kTradeCursor = bof3::addr::ItemTrade_DrawNeeds;     // no arguments: a window drawn
constexpr std::uint32_t kTradeFrame = bof3::addr::ItemTrade_DrawBackground;      // no arguments: a window frame drawn
constexpr std::uint32_t kTradeRows = bof3::addr::ItemTrade_RowCount;       // al: the row count, stored at 0x6BE08D
constexpr std::uint32_t kTradeLacks = bof3::addr::ItemTrade_Lacks;      // (row byte, quantity byte), al: 1 when an ingredient falls short
constexpr std::uint32_t kTradeCount = bof3::addr::ItemTrade_DrawCount;      // no arguments: a window drawn
constexpr std::uint32_t kTradeTake = bof3::addr::ItemTrade_TakeNeeds;       // no arguments: the ingredients taken (Inventory_Remove behind a test)

// --- the event records (save block +0xCF0..) ----------------------------------
constexpr std::uint32_t kPairs = 0x9046D0;           // 60 x 8 bytes: +0 in use, +1 the record it names (1-based)
constexpr std::uint32_t kPairsEnd = 0x9048B0;
constexpr std::uint32_t kRecords = 0x9048B0;         // 8 x 8 bytes: +0 kind, +1 key, +3 counted, dword +4 the tally
// kStoryFlags: now Cond_Flags[0xa0] (symbols.toml). Cond_Flags row 0x14: the story flags
constexpr std::uint32_t kTallyBits = 0x904654;       // a bit array (bit 4 at +0 and at +3 tested)
// kTallyByte: now AreaW1c_Byte9045FB (symbols.toml). u8: counted up to 0x1E
// kTallyDword: now Rest4a_ClockA (symbols.toml). u32: the count toward the next story flag
// kTallyA: now Rest4a_StampLevelLit (symbols.toml). u32: Records_CountUnpaired's counter for key 1
// kTallyB: now Rest4a_StampLevelDark (symbols.toml). u32: its counter for key 0
// kLevelA: now FieldE2_LevelA (symbols.toml). u8: 0 - no story tally; below 10 - key 0 counted
// kLevelB: now Rest4a_LevelLit (symbols.toml). u8: below 7 - key 1 counted; 4 or more - story flag 0xF5
// kBiasA: now FieldE2_BiasA (symbols.toml). u8: subtracted from the thresholds of flags 0xF6, 0xF9, 0xFC
// kBiasB: now FieldE2_BiasB (symbols.toml). u8: from the others'

// --- the floor, the leader ---------------------------------------------------------
constexpr std::uint32_t kCharRecords = 0x903A70;     // CharacterRecords (0xA4 each)
constexpr std::uint32_t kRecordStride = 0xA4;
constexpr std::uint32_t kEffectSlot = 0x903850;      // DamageScratch's byte: the slope probe's flag; the party-set ids 0x903850..52
constexpr std::uint32_t kDirectionSteps = 0x6697B0;  // Field_DirectionSteps: 8 x (dx, dz) dwords
constexpr std::uint32_t kPaceHeights = 0x66978C;     // s16 per actor id: the ground's offset for a hop
constexpr std::uint32_t kPaceRises = 0x6697A4;       // u8 per actor id: a step's rise
// kClutWords: now Gfx_ClutStrip[0xf1f] (symbols.toml). u16 per member slot (0x40 apart): cleared by the hop
// kPalettes: now Gfx_ClutStripSource[0xf00] (symbols.toml). 0x40 bytes per member slot: Sprite_LoadPalette's source
constexpr std::uint32_t kScriptFlagsLow = 0x9039A2;  // Field_ScriptFlags' byte 2 (bit 3 with Field_ScriptFlags2's)

// --- the mode-11 object ------------------------------------------------------------
constexpr std::uint32_t kObject = 0x905DA0;          // the object record (0xA4) mode 11 moves
// kObjectStates: now Mode11_ObjectStates (symbols.toml). Mode11_ObjectStates, 4 entries
constexpr std::uint32_t kButtonsRun = 0x903582;      // u16: the run buttons (save data)
constexpr std::uint32_t kButtonsLeave = 0x903586;    // u16: the buttons that leave mode 11 (save data)
// kRunDefault: now FieldE2_RunDefault (symbols.toml). u8: the run toggle
// kLeaderX, kLeaderZ, kLeaderY: now rv::As<records::PartyMember>(ObjTrio)->x / z / height (records.h). ObjTrio[0] +0x34 / +0x38 / +0x3C / +0x138
// kLeaderFlags: now ObjTrio[0x138] (symbols.toml).
constexpr std::uint32_t kInputHeld = 0x905BA6;       // Field_InputHeld
constexpr std::uint32_t kScriptFlags2 = 0x905BA4;    // Field_ScriptFlags2
// kKind2Z: now Field_Kind2Z (symbols.toml). Field_Kind2Z / X
// kKind2X: now Field_Kind2X (symbols.toml).
// kKind2Hold: now Field_Kind2Hold (symbols.toml). Field_Kind2Hold
// kF3Divisor: now MoveScript_F3Divisor (symbols.toml). MoveScript_F3Divisor (u16)
// kFaWord: now MoveScript_FAWord (symbols.toml). MoveScript_FAWord (u16)
constexpr std::uint32_t kElevation = 0x929F1C;       // MapView_Elevation (u16)
// kGameStep: now Game_Step (symbols.toml). Game_Step (u16)
// kVertexScratch: now Prim_VertexScratch (symbols.toml). Prim_VertexScratch: three SVECTORs
constexpr std::uint32_t kCameraMatrix = 0x905E40;    // Camera_Matrix
constexpr rdata::Const kQuadLift{0x5C4254};        // float: the marker quad's height
constexpr rdata::Const kQuadWidth{0x5C41F0};       // float: its width

// --- the party-set error screen -------------------------------------------------------
constexpr std::uint32_t kErrorTitle = 0x660C90;      // the title line it prints
constexpr std::uint32_t kErrorFormat = 0x660C7C;     // the three ids' format
constexpr std::uint32_t kErrorIds = 0x903850;        // the three ids PartySet_Find left
constexpr std::uint32_t kTextScratch = 0x904BA0;

// --- the object triggers, the hooks -------------------------------------------------------
// kTriggers: now WorldMap_FieldHooks[0xb] (symbols.toml). Field_ObjectTriggers - 4: entry n of object +0x86 is Field_ObjectTriggers[n - 1]

// --- the field tail -----------------------------------------------------------------------
// kTailKind: now AreaW0a_TailKind (symbols.toml). s8: Field_ModeTailKinds' index
// kTailState: now AreaW0c_TailState (symbols.toml). s8: the tail kind's state
// kTailArg: now AreaW0c_TailSub (symbols.toml). u8: its argument
constexpr std::uint32_t kTailTimer = 0x9039F6;       // u16
// kPartySet: now AreaW0b_PartySet (symbols.toml). u8: the current party set (bit 7 set by the bank load)
constexpr std::uint32_t kDropInTable = 0x662DE8;     // u8 per argument: Party_DropIn's entry
constexpr std::uint32_t kCounterB = 0x90384B;        // the chapters' counter byte the drop-in waits on
// kPassFlags: now Draw_PassFlags (symbols.toml). Draw_PassFlags
// kMenuMode: now BattleE1_Mode (symbols.toml).
// kMenuCursor: now ModeStates_ShopObject (symbols.toml).
constexpr std::uint32_t kFieldEC2 = 0x929EC2;
// kFieldEC3: now ScenaSc1_ShopByteB (symbols.toml).
// kGameMode: now Game_Mode (symbols.toml). Game_Mode (u16)
// kCondByteFE: now Cond_ByteFE (symbols.toml). Cond_ByteFE
// kStreamFlag: now AreaW1a_FocusObject (symbols.toml). a pointer to the byte FieldTail_FlagMessage clears
// kMessageBits: now ScenaSc7_EventFlags (symbols.toml). the bits FieldTail_FlagMessage sets by its argument
// kReturnX: now AreaW2d_ReturnPoint (symbols.toml). FieldTail_StoryWarp's saved leader x, z, area
// kReturnZ: now AreaW4d_ReturnZ (symbols.toml).
// kReturnArea: now AreaW4d_ReturnArea (symbols.toml).
constexpr std::uint32_t kTextRecords = 0x904CE0;     // Text_Records

// --- the map --------------------------------------------------------------------------------
// kMapOrigin: now MapView_Origin (symbols.toml). MapView_Origin: two words
constexpr std::uint32_t kMapCells = 0x904F20;        // MapView_Cells: 0x38 rows of 0x1C words
constexpr std::uint32_t kCellRecords = 0x8CB580;     // AreaMap_Header: the records' dwords
// kCellBase: now AreaMap_CellBase (symbols.toml). AreaMap_CellBase (u16 used)
constexpr std::uint32_t kOtSlot = 0x92BF19;          // Draw_OtSlot
constexpr unsigned kPrimBytes = 0x48;                // a POLY_FT4 as the port lays it out

// --- the trade screen ---------------------------------------------------------------------------
// kTradeState: now Effect1g_TradeState (symbols.toml). u8: ItemTrade_States' index (0x593950 dispatches)
constexpr std::uint32_t kTradeStep = 0x93985E;       // u8: the open and run steps' index
// kTradeOpenSteps: now ItemTrade_OpenSteps (symbols.toml). ItemTrade_OpenSteps, 2
// kTradeRunSteps: now ItemTrade_RunSteps (symbols.toml). ItemTrade_RunSteps, 4 (the fourth, 0x594060, nobody's)
constexpr std::uint32_t kTradePick = 0x6BE08C;       // u8: the row picked (bit 7, bit 6 marks)
// kTradeRowCount: now Effect1g_TradeRowCount (symbols.toml). u8
// kTradeQuantity: now Effect1g_TradeQuantity (symbols.toml). u8: 1..99
// kTradeAnswer: now Effect1g_TradeAnswer (symbols.toml). u8: the yes / no hand
// kTradeRow: now AreaEntry_PendingFlags (symbols.toml). u8: which ten-entry row of 0x66AD10 the screen shows
constexpr std::uint32_t kTradeIndex = 0x66AD10;      // 10 bytes a row: the entries' record numbers
constexpr std::uint32_t kTradeRecords = 0x66AB58;    // 8 bytes a record: category, item, ...
// kConfirm: now Field_ConfirmButtons (symbols.toml). Field_ConfirmButtons
// kCancel: now Field_CancelButtons (symbols.toml). Field_CancelButtons
constexpr std::uint32_t kInputPressed = 0x7E1BEC;    // Input_Pressed (u16 read)
constexpr std::uint32_t kPoolWordPick = 0x80361C;    // MessagePools' offsets of three prompts
constexpr std::uint32_t kPoolWordName = 0x80360C;
constexpr std::uint32_t kPoolWordAsk = 0x803614;

// Named data whose symbols.gen.h name is a macro (bof3::addr::<Name> would expand it).
constexpr std::uint32_t kScenarioHooks = 0x662C80;   // Scenario_Hooks
// kWorldMapHooks: now WorldMap_FieldHooks (symbols.toml). WorldMap_FieldHooks
// kWaitWord: now MoveScript_WaitWordDA (symbols.toml). MoveScript_WaitWordDA (u16)
// kAreaNumber: now Game_AreaNumber (symbols.toml). Game_AreaNumber (u16)
// kMapRow: now MapView_Row (symbols.toml). MapView_Row (s16)
// kMapColumn: now MapView_Column (symbols.toml). MapView_Column (s16)

constexpr std::uint32_t kTextLo = 0x401000, kTextHi = 0x5C3000;   // .text

}  // namespace field_e2::at
