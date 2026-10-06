// Group R2C's raw addresses (round fourteen, wave two; docs/rest_2c.md): the
// .data tables its dispatchers jump through (each a [[data]] entry in
// symbols.toml with the count its own reader reaches, section 3 of the doc),
// the cells its code names by address, and the three callees of other groups
// of this wave, called by address (ours since their owners merged, named by
// symbol, the values unchanged: round fourteen's rebinding,
// docs/round-14-cleanup.md).
#pragma once

#include <cstdint>

#include "bof3/symbols.gen.h"

namespace rest_2c {
namespace at {

// --- the state tables the group's dispatchers read (in address order) ---------
constexpr std::uint32_t kRestStates = 0x663FB8;          // Rest_States: 7, by 0x929F01 (Rest_Dispatch)
constexpr std::uint32_t kPartyFormStates = 0x66409C;     // PartyForm_States: 4, by 0x929F01 (PartyForm_Dispatch)
constexpr std::uint32_t kShopSellStates = 0x66416C;      // ShopSell_States: 4, by 0x929F01 (ShopSell_Dispatch)
constexpr std::uint32_t kShopSellSellSteps = 0x66417C;   // ShopSell_SellSteps: 5, by 0x929F02 (ShopSell_SellStep)
constexpr std::uint32_t kBrowseStates = 0x664190;        // ShopBrowse_States: 3, by 0x929F01 (ShopBrowse_Dispatch)
constexpr std::uint32_t kBrowseOpenSteps = 0x66419C;     // ShopBrowse_OpenSteps: 2, by 0x929F02
constexpr std::uint32_t kBrowseChooseSteps = 0x6641A4;   // ShopBrowse_ChooseSteps: 3, by 0x929F02
constexpr std::uint32_t kBrowseCloseSteps = 0x6641B0;    // ShopBrowse_CloseSteps: 3, by 0x929F02
constexpr std::uint32_t kResistStates = 0x6641BC;        // ShopResist_States: 8, by 0x929F01 (ShopResist_Dispatch)
constexpr std::uint32_t kSharedListStates = 0x664254;    // SharedList_States: 5, by 0x929F01 (SharedList_Dispatch)
constexpr std::uint32_t kTalkStates = 0x66450C;          // MasterTalk_States: 7, by 0x9398CF (MasterTalk_Dispatch)
constexpr std::uint32_t kTalkIntroSteps = 0x664528;      // MasterTalk_IntroSteps: 2, by 0x9398D1
constexpr std::uint32_t kTalkAskSteps = 0x664530;        // MasterTalk_AskSteps: 6, by 0x9398D1
constexpr std::uint32_t kTalkPickSteps = 0x664548;       // MasterTalk_PickSteps: 9, by 0x9398D1
// A table the group's functions sit in but no code of the group reads:
constexpr std::uint32_t kFigureStates = 0x663E28;        // MasterFigure_States: 6, by 0x9398E1 (R2B's 0x57F320)

// --- the menu block and the shop's cells ---------------------------------------
constexpr std::uint32_t kMode = 0x929F00;        // ShopMode_States' index
constexpr std::uint32_t kState = 0x929F01;       // the mode's state
constexpr std::uint32_t kStep = 0x929F02;        // the state's step
constexpr std::uint32_t kTimer = 0x929F04;       // a frame counter
constexpr std::uint32_t kAnswer = 0x929F0B;      // a yes / no answer (0 the first)
constexpr std::uint32_t kObject = 0x929F0C;      // the touched object (0xFF none, 0xFE the save point)
constexpr std::uint32_t kKeptObject = 0x929F0F;  // the object Inn_Begin kept
constexpr std::uint32_t kMemberCount = 0x929EC0; // Field_MemberCount
constexpr std::uint32_t kColour = 0x903A5A;      // the window style byte
constexpr std::uint32_t kZenny = 0x904058;       // Party_Zenny
constexpr std::uint32_t kPressed = 0x7E1BEC;     // Input_Pressed's low word
constexpr std::uint32_t kConfirm = 0x90358E;     // Field_ConfirmButtons (u16)
constexpr std::uint32_t kCancel = 0x903590;      // Field_CancelButtons (u16)
constexpr std::uint32_t kMsgFlags = 0x7DEE44;    // the message box's flags (bit 1: done)
constexpr std::uint32_t kInputFlags = 0x905BA2;  // Field_InputFlags
constexpr std::uint32_t kArea = 0x904EFC;        // Game_AreaNumber (u16)
constexpr std::uint32_t kWait = 0x66C810;        // MoveScript_WaitWordDA (u16)
constexpr std::uint32_t kRequest = 0x66C7D8;     // Field_Request (u8)
constexpr std::uint32_t kSaveBack = 0x6BC880;    // the save menu's "saved" byte (FieldSave_End reads it)
constexpr std::uint32_t kInnCursor = 0x6BC881;   // the inn prompt's cursor
constexpr std::uint32_t kSellFlag = 0x6BC8A4;    // cleared by ShopSell_Open
constexpr std::uint32_t kSellOn = 0x6BC8AA;      // set by ShopSell_Open

// --- the save ---------------------------------------------------------------------
constexpr std::uint32_t kSlot = 0x9036D4;        // u32: the save cursor, 0..15
constexpr std::uint32_t kPath = 0x904BA0;        // the text scratch: the save file's name
constexpr std::uint32_t kPathFormat = 0x664068;  // its format (one number: the slot)
constexpr std::uint32_t kSaveSize = 0x12B0;      // Save_WriteFile's size
constexpr std::uint32_t kSummary = 0x904680;     // the summary the block builder fills (7 dwords)
constexpr std::uint32_t kSummaries = 0x905BC0;   // 16 x 0x1C: the slots' summaries
constexpr std::uint32_t kSummaryBytes = 0x1C;
constexpr unsigned kSlotCount = 16;
constexpr std::uint32_t kBlock = 0x9039E0;       // the live save block, 0x10B0 bytes
constexpr std::uint32_t kBlockBytes = 0x10B0;
constexpr std::uint32_t kStaging = 0x92A0E0;     // its copy; then 0xD50 bytes cleared (0x92B190..0x92BEE0)
constexpr std::uint32_t kStagingClear = 0xD50;
constexpr std::uint32_t kChecksum = 0x92A150;    // u16: the bytes' sum (the copy of the block's +0x70)
constexpr std::uint32_t kMemberRecord = 0x66972C; // MoveScript_EffectState: a party id's record index
constexpr std::uint32_t kRecords = 0x903A70;     // the character records, 0xA4 each
constexpr std::uint32_t kRecordStride = 0xA4;
constexpr std::uint32_t kPartyList = 0x904062;   // the party's three ids
constexpr std::uint32_t kStoryByte = 0x90412C;   // its low 7 bits compared with 5 and 0xC (id 4's form)
constexpr std::uint32_t kStoryFlags = 0x904030;  // the story flags (Flags_Test row)

// --- the master's cells --------------------------------------------------------------
constexpr std::uint32_t kMaster = 0x9039F5;      // the master spoken to (the per-master message table's index)
constexpr std::uint32_t kMetFlags = 0x904654;    // Flags_Set by master when the introduction opens
constexpr std::uint32_t kIntroFlags = 0x904657;  // Flags_Test / Flags_Set by master: the introduction done
constexpr std::uint32_t kMasterMessages = 0x6644E8;  // u16 per master: its script messages' base
constexpr std::uint32_t kTalkMode = 0x9398CF;    // MasterTalk_States' index
constexpr std::uint32_t kTalkStep = 0x9398D1;    // its step
constexpr std::uint32_t kTalkSlide = 0x9398D3;   // the panels' slide counter (4 out .. 0 in)
constexpr std::uint32_t kTalkPick = 0x9398CE;    // the pick (cleared when the panels are in)
constexpr std::uint32_t kTalkD0 = 0x9398D0;      // cleared by MasterTalk_Reset
constexpr std::uint32_t kTalkCD = 0x9398CD;      // cleared by MasterTalk_Reset
constexpr std::uint32_t kMembers = 0x802E88;     // ObjTrio + 0x148: Field_Members' record index, 0x14C apart
constexpr std::uint32_t kObjStride = 0x14C;
constexpr std::uint32_t kExpTable = 0x658F48;    // Char_ExpTable: 0x318 bytes a member, a u16 every 8
constexpr std::uint32_t kFaceTable = 0x6644B8;   // 4 bytes an id: u, v, the CLUT's x << 4, its row
constexpr std::uint32_t kStatLabels = 0x66A0F8;  // DIV-0064's four menu stat labels, 8 apart
constexpr std::uint32_t kStatusA = 0x66A0E8;     // DIV-0064's status words
constexpr std::uint32_t kStatusB = 0x66A0F0;
constexpr std::uint32_t kFmtNumber = 0x64E324;   // a number's format
constexpr std::uint32_t kFmtLeft = 0x6639B0;     // a figure's (the current)
constexpr std::uint32_t kFmtRight = 0x6639A8;    // and its maximum's
constexpr std::uint32_t kStatsPieces = 0x664468; // Menu_DrawPieces lists: the stats box
constexpr std::uint32_t kStatsRow = 0x664494;    // its row, sixteen times 8 apart
constexpr std::uint32_t kStatsEnd = 0x6644A0;
constexpr std::uint32_t kMemberPieces = 0x6643D8; // the member panel's
constexpr std::uint32_t kFrameCounter = 0x937F94; // Frame_Counter (bit 5: the status words' blink)
constexpr std::uint32_t kPacketNext = 0x7E0670;  // Gfx_PacketNext
constexpr std::uint32_t kBoxAdd = 0x5C41C0;      // the panel box's three float constants (read in place)
constexpr std::uint32_t kBoxSub = 0x5C41BC;
constexpr std::uint32_t kBoxSubBottom = 0x5C41B8;

// --- model A (0x9398E0, dispatched by R2B's 0x57F320 on +1), and what follows it ----
// R2B's layout (docs/rest_2c.md 1.1): model A 0x80 bytes, model B 0x939960, then
// the screen's cells 0x9399E0.. - the cells past +0x80 below are not A's.
constexpr std::uint32_t kFigure = 0x9398E0;
constexpr std::uint32_t kFigureDone = 0x9398E6;  // +6: 1 when a move reached its end
constexpr std::uint32_t kFigureY = 0x93991C;     // +0x3C: the height, 16.16
constexpr std::uint32_t kFigureScale = 0x939920; // +0x40
constexpr std::uint32_t kFigureRgb = 0x93993D;   // +0x5D..+0x5F: the colour
constexpr std::uint32_t kFigureAngle = 0x93994C; // +0x6C
constexpr std::uint32_t kFigureLift = 0x9399A0;  // model B's scale (+0x40): A stands on B, in 0xA00ths
constexpr std::uint32_t kFigureFade = 0x9399E7;  // the fourth "given" count (R2B's 0x9399E4 + 3): the fade's step
constexpr std::uint32_t kFigureScaleIndex = 0x9399EB;  // R2B's level (Shisu_ScaleIndex's answer)
constexpr std::uint32_t kScaleTable = 0x663D7C;  // the scales the index reads (in place)
constexpr std::uint32_t kKind2X = 0x905E64;      // Field_Kind2X
constexpr std::uint32_t kKind2Z = 0x905E60;      // Field_Kind2Z

// --- the windows (WindowRecords 0x803160, 0x24 each) ---------------------------------
constexpr std::uint32_t kWindows = 0x803160;
constexpr std::uint32_t kWindowStride = 0x24;

// --- callees another group of this wave owns, and the C runtime's ------------------
constexpr std::uint32_t kFigureDraw = bof3::addr::Shisu_DrawModel;   // R2B: the figure record drawn (one word: the record)
constexpr std::uint32_t kPickAsk = bof3::addr::MasterScreen_PickMember;      // R2D: the master's pick, (message u16, a byte)
constexpr std::uint32_t kGlyph = bof3::addr::BattleEquipWin_DrawBar;        // R2H: an 8 x 8 cell (x, y, u / 8, v / 8, clut, shade)

}  // namespace at
}  // namespace rest_2c
