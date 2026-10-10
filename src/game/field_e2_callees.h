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
constexpr std::uint32_t kStoryFlags = 0x904030;      // Cond_Flags row 0x14: the story flags
constexpr std::uint32_t kTallyBits = 0x904654;       // a bit array (bit 4 at +0 and at +3 tested)
constexpr std::uint32_t kTallyByte = 0x9045FB;       // u8: counted up to 0x1E
constexpr std::uint32_t kTallyDword = 0x9046B0;      // u32: the count toward the next story flag
constexpr std::uint32_t kTallyA = 0x9046C0;          // u32: Records_CountUnpaired's counter for key 1
constexpr std::uint32_t kTallyB = 0x9046C4;          // u32: its counter for key 0
constexpr std::uint32_t kLevelA = 0x9046CB;          // u8: 0 - no story tally; below 10 - key 0 counted
constexpr std::uint32_t kLevelB = 0x9046CC;          // u8: below 7 - key 1 counted; 4 or more - story flag 0xF5
constexpr std::uint32_t kBiasA = 0x9046CD;           // u8: subtracted from the thresholds of flags 0xF6, 0xF9, 0xFC
constexpr std::uint32_t kBiasB = 0x9046CE;           // u8: from the others'

// --- the floor, the leader ---------------------------------------------------------
constexpr std::uint32_t kCharRecords = 0x903A70;     // CharacterRecords (0xA4 each)
constexpr std::uint32_t kRecordStride = 0xA4;
constexpr std::uint32_t kEffectSlot = 0x903850;      // DamageScratch's byte: the slope probe's flag; the party-set ids 0x903850..52
constexpr std::uint32_t kDirectionSteps = 0x6697B0;  // Field_DirectionSteps: 8 x (dx, dz) dwords
constexpr std::uint32_t kPaceHeights = 0x66978C;     // s16 per actor id: the ground's offset for a hop
constexpr std::uint32_t kPaceRises = 0x6697A4;       // u8 per actor id: a step's rise
constexpr std::uint32_t kClutWords = 0x8113BE;       // u16 per member slot (0x40 apart): cleared by the hop
constexpr std::uint32_t kPalettes = 0x80D380;        // 0x40 bytes per member slot: Sprite_LoadPalette's source
constexpr std::uint32_t kScriptFlagsLow = 0x9039A2;  // Field_ScriptFlags' byte 2 (bit 3 with Field_ScriptFlags2's)

// --- the mode-11 object ------------------------------------------------------------
constexpr std::uint32_t kObject = 0x905DA0;          // the object record (0xA4) mode 11 moves
constexpr std::uint32_t kObjectStates = 0x660C2C;    // Mode11_ObjectStates, 4 entries
constexpr std::uint32_t kButtonsRun = 0x903582;      // u16: the run buttons (save data)
constexpr std::uint32_t kButtonsLeave = 0x903586;    // u16: the buttons that leave mode 11 (save data)
constexpr std::uint32_t kRunDefault = 0x903A5E;      // u8: the run toggle
constexpr std::uint32_t kLeaderX = 0x802D74;         // ObjTrio[0] +0x34 / +0x38 / +0x3C / +0x138
constexpr std::uint32_t kLeaderZ = 0x802D78;
constexpr std::uint32_t kLeaderY = 0x802D7C;
constexpr std::uint32_t kLeaderFlags = 0x802E78;
constexpr std::uint32_t kInputHeld = 0x905BA6;       // Field_InputHeld
constexpr std::uint32_t kScriptFlags2 = 0x905BA4;    // Field_ScriptFlags2
constexpr std::uint32_t kKind2Z = 0x905E60;          // Field_Kind2Z / X
constexpr std::uint32_t kKind2X = 0x905E64;
constexpr std::uint32_t kKind2Hold = 0x929F12;       // Field_Kind2Hold
constexpr std::uint32_t kF3Divisor = 0x937F8C;       // MoveScript_F3Divisor (u16)
constexpr std::uint32_t kFaWord = 0x904EFE;          // MoveScript_FAWord (u16)
constexpr std::uint32_t kElevation = 0x929F1C;       // MapView_Elevation (u16)
constexpr std::uint32_t kGameStep = 0x66C7EA;        // Game_Step (u16)
constexpr std::uint32_t kVertexScratch = 0x9037A0;   // Prim_VertexScratch: three SVECTORs
constexpr std::uint32_t kCameraMatrix = 0x905E40;    // Camera_Matrix
constexpr rdata::Const kQuadLift{0x5C4254};        // float: the marker quad's height
constexpr rdata::Const kQuadWidth{0x5C41F0};       // float: its width

// --- the party-set error screen -------------------------------------------------------
constexpr std::uint32_t kErrorTitle = 0x660C90;      // the title line it prints
constexpr std::uint32_t kErrorFormat = 0x660C7C;     // the three ids' format
constexpr std::uint32_t kErrorIds = 0x903850;        // the three ids PartySet_Find left
constexpr std::uint32_t kTextScratch = 0x904BA0;

// --- the object triggers, the hooks -------------------------------------------------------
constexpr std::uint32_t kTriggers = 0x662E1C;        // Field_ObjectTriggers - 4: entry n of object +0x86 is Field_ObjectTriggers[n - 1]

// --- the field tail -----------------------------------------------------------------------
constexpr std::uint32_t kTailKind = 0x9039F3;        // s8: Field_ModeTailKinds' index
constexpr std::uint32_t kTailState = 0x9039F4;       // s8: the tail kind's state
constexpr std::uint32_t kTailArg = 0x9039F5;         // u8: its argument
constexpr std::uint32_t kTailTimer = 0x9039F6;       // u16
constexpr std::uint32_t kPartySet = 0x90412C;        // u8: the current party set (bit 7 set by the bank load)
constexpr std::uint32_t kDropInTable = 0x662DE8;     // u8 per argument: Party_DropIn's entry
constexpr std::uint32_t kCounterB = 0x90384B;        // the chapters' counter byte the drop-in waits on
constexpr std::uint32_t kPassFlags = 0x7E0918;       // Draw_PassFlags
constexpr std::uint32_t kMenuMode = 0x929F00;
constexpr std::uint32_t kMenuCursor = 0x929F0C;
constexpr std::uint32_t kFieldEC2 = 0x929EC2;
constexpr std::uint32_t kFieldEC3 = 0x929EC3;
constexpr std::uint32_t kGameMode = 0x66C7E8;        // Game_Mode (u16)
constexpr std::uint32_t kCondByteFE = 0x905E20;      // Cond_ByteFE
constexpr std::uint32_t kStreamFlag = 0x903804;      // a pointer to the byte FieldTail_FlagMessage clears
constexpr std::uint32_t kMessageBits = 0x904650;     // the bits FieldTail_FlagMessage sets by its argument
constexpr std::uint32_t kReturnX = 0x904148;         // FieldTail_StoryWarp's saved leader x, z, area
constexpr std::uint32_t kReturnZ = 0x90414C;
constexpr std::uint32_t kReturnArea = 0x904150;
constexpr std::uint32_t kTextRecords = 0x904CE0;     // Text_Records

// --- the map --------------------------------------------------------------------------------
constexpr std::uint32_t kMapOrigin = 0x7E0688;       // MapView_Origin: two words
constexpr std::uint32_t kMapCells = 0x904F20;        // MapView_Cells: 0x38 rows of 0x1C words
constexpr std::uint32_t kCellRecords = 0x8CB580;     // AreaMap_Header: the records' dwords
constexpr std::uint32_t kCellBase = 0x8CB5A4;        // AreaMap_CellBase (u16 used)
constexpr std::uint32_t kOtSlot = 0x92BF19;          // Draw_OtSlot
constexpr unsigned kPrimBytes = 0x48;                // a POLY_FT4 as the port lays it out

// --- the trade screen ---------------------------------------------------------------------------
constexpr std::uint32_t kTradeState = 0x93985C;      // u8: ItemTrade_States' index (0x593950 dispatches)
constexpr std::uint32_t kTradeStep = 0x93985E;       // u8: the open and run steps' index
constexpr std::uint32_t kTradeOpenSteps = 0x66A47C;  // ItemTrade_OpenSteps, 2
constexpr std::uint32_t kTradeRunSteps = 0x66A484;   // ItemTrade_RunSteps, 4 (the fourth, 0x594060, nobody's)
constexpr std::uint32_t kTradePick = 0x6BE08C;       // u8: the row picked (bit 7, bit 6 marks)
constexpr std::uint32_t kTradeRowCount = 0x6BE08D;   // u8
constexpr std::uint32_t kTradeQuantity = 0x6BE08E;   // u8: 1..99
constexpr std::uint32_t kTradeAnswer = 0x6BE08F;     // u8: the yes / no hand
constexpr std::uint32_t kTradeRow = 0x905B88;        // u8: which ten-entry row of 0x66AD10 the screen shows
constexpr std::uint32_t kTradeIndex = 0x66AD10;      // 10 bytes a row: the entries' record numbers
constexpr std::uint32_t kTradeRecords = 0x66AB58;    // 8 bytes a record: category, item, ...
constexpr std::uint32_t kConfirm = 0x90358E;         // Field_ConfirmButtons
constexpr std::uint32_t kCancel = 0x903590;          // Field_CancelButtons
constexpr std::uint32_t kInputPressed = 0x7E1BEC;    // Input_Pressed (u16 read)
constexpr std::uint32_t kPoolWordPick = 0x80361C;    // MessagePools' offsets of three prompts
constexpr std::uint32_t kPoolWordName = 0x80360C;
constexpr std::uint32_t kPoolWordAsk = 0x803614;

// Named data whose symbols.gen.h name is a macro (bof3::addr::<Name> would expand it).
constexpr std::uint32_t kScenarioHooks = 0x662C80;   // Scenario_Hooks
constexpr std::uint32_t kWorldMapHooks = 0x662DF0;   // WorldMap_FieldHooks
constexpr std::uint32_t kWaitWord = 0x66C810;        // MoveScript_WaitWordDA (u16)
constexpr std::uint32_t kAreaNumber = 0x904EFC;      // Game_AreaNumber (u16)
constexpr std::uint32_t kMapRow = 0x929F24;          // MapView_Row (s16)
constexpr std::uint32_t kMapColumn = 0x929F20;       // MapView_Column (s16)

constexpr std::uint32_t kTextLo = 0x401000, kTextHi = 0x5C3000;   // .text

}  // namespace field_e2::at
