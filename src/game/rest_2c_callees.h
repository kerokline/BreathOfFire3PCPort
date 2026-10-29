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
#include "game/rdata_consts.h"

namespace rest_2c {
namespace at {

// --- the state tables the group's dispatchers read (in address order) ---------
// kRestStates: now Rest_States (symbols.toml). Rest_States: 7, by 0x929F01 (Rest_Dispatch)
// kPartyFormStates: now PartyForm_States (symbols.toml). PartyForm_States: 4, by 0x929F01 (PartyForm_Dispatch)
// kShopSellStates: now ShopSell_States (symbols.toml). ShopSell_States: 4, by 0x929F01 (ShopSell_Dispatch)
// kShopSellSellSteps: now ShopSell_SellSteps (symbols.toml). ShopSell_SellSteps: 5, by 0x929F02 (ShopSell_SellStep)
// kBrowseStates: now ShopBrowse_States (symbols.toml). ShopBrowse_States: 3, by 0x929F01 (ShopBrowse_Dispatch)
// kBrowseOpenSteps: now ShopBrowse_OpenSteps (symbols.toml). ShopBrowse_OpenSteps: 2, by 0x929F02
// kBrowseChooseSteps: now ShopBrowse_ChooseSteps (symbols.toml). ShopBrowse_ChooseSteps: 3, by 0x929F02
// kBrowseCloseSteps: now ShopBrowse_CloseSteps (symbols.toml). ShopBrowse_CloseSteps: 3, by 0x929F02
// kResistStates: now ShopResist_States (symbols.toml). ShopResist_States: 8, by 0x929F01 (ShopResist_Dispatch)
// kSharedListStates: now SharedList_States (symbols.toml). SharedList_States: 5, by 0x929F01 (SharedList_Dispatch)
// kTalkStates: now MasterTalk_States (symbols.toml). MasterTalk_States: 7, by 0x9398CF (MasterTalk_Dispatch)
// kTalkIntroSteps: now MasterTalk_IntroSteps (symbols.toml). MasterTalk_IntroSteps: 2, by 0x9398D1
// kTalkAskSteps: now MasterTalk_AskSteps (symbols.toml). MasterTalk_AskSteps: 6, by 0x9398D1
// kTalkPickSteps: now MasterTalk_PickSteps (symbols.toml). MasterTalk_PickSteps: 9, by 0x9398D1
// A table the group's functions sit in but no code of the group reads:
constexpr ::bof3::image::Va kFigureStates{0x663E28};        // MasterFigure_States: 6, by 0x9398E1 (R2B's 0x57F320)

// --- the menu block and the shop's cells ---------------------------------------
// kMode: now BattleE1_Mode (symbols.toml). ShopMode_States' index
constexpr ::bof3::image::Va kState{0x929F01};       // the mode's state
// kStep: now FieldS_Step (symbols.toml). the state's step
constexpr ::bof3::image::Va kTimer{0x929F04};       // a frame counter
// kAnswer: now FieldS_Answer (symbols.toml). a yes / no answer (0 the first)
// kObject: now ModeStates_ShopObject (symbols.toml). the touched object (0xFF none, 0xFE the save point)
// kKeptObject: now AreaW3c_Load0F (symbols.toml). the object Inn_Begin kept
// kMemberCount: now Field_MemberCount (symbols.toml). Field_MemberCount
// kColour: now Config_WindowColour (symbols.toml). the window style byte
// kZenny: now Party_Zenny (symbols.toml). Party_Zenny
// kPressed: now Input_Pressed (symbols.toml). Input_Pressed's low word
// kConfirm: now Field_ConfirmButtons (symbols.toml). Field_ConfirmButtons (u16)
// kCancel: now Field_CancelButtons (symbols.toml). Field_CancelButtons (u16)
constexpr ::bof3::image::Va kMsgFlags{0x7DEE44};    // the message box's flags (bit 1: done)
// kInputFlags: now Field_InputFlags (symbols.toml). Field_InputFlags
// kArea: now Game_AreaNumber (symbols.toml). Game_AreaNumber (u16)
// kWait: now Transition_Busy (symbols.toml). Transition_Busy (u16)
// kRequest: now Field_Request (symbols.toml). Field_Request (u8)
// kSaveBack: now Rest2c_SaveBack (symbols.toml). the save menu's "saved" byte (FieldSave_End reads it)
// kInnCursor: now ShopStates_Choice (symbols.toml). the inn prompt's cursor
// kSellFlag: now Rest2c_SellFlag (symbols.toml). cleared by ShopSell_Open
// kSellOn: now ShopStates2_SellFlag (symbols.toml). set by ShopSell_Open

// --- the save ---------------------------------------------------------------------
// kSlot: now Rest2c_Slot (symbols.toml). u32: the save cursor, 0..15
constexpr ::bof3::image::Va kPath{0x904BA0};        // the text scratch: the save file's name
constexpr ::bof3::image::Va kPathFormat{0x664068};  // its format (one number: the slot)
constexpr std::uint32_t kSaveSize = 0x12B0;      // Save_WriteFile's size
constexpr ::bof3::image::Va kSummary{0x904680};     // the summary the block builder fills (7 dwords)
constexpr ::bof3::image::Va kSummaries{0x905BC0};   // 16 x 0x1C: the slots' summaries
constexpr std::uint32_t kSummaryBytes = 0x1C;
constexpr unsigned kSlotCount = 16;
constexpr ::bof3::image::Va kBlock{0x9039E0};       // the live save block, 0x10B0 bytes
constexpr std::uint32_t kBlockBytes = 0x10B0;
constexpr ::bof3::image::Va kStaging{0x92A0E0};     // its copy; then 0xD50 bytes cleared (0x92B190..0x92BEE0)
constexpr std::uint32_t kStagingClear = 0xD50;
constexpr ::bof3::image::Va kChecksum{0x92A150};    // u16: the bytes' sum (the copy of the block's +0x70)
constexpr ::bof3::image::Va kMemberRecord{0x66972C}; // Character_IndexById: a party id's record index
constexpr ::bof3::image::Va kRecords{0x903A70};     // the character records, 0xA4 each
constexpr std::uint32_t kRecordStride = 0xA4;
constexpr ::bof3::image::Va kPartyList{0x904062};   // the party's three ids
// kStoryByte: now AreaW0b_PartySet (symbols.toml). its low 7 bits compared with 5 and 0xC (id 4's form)
// kStoryFlags: now Cond_Flags[0xa0] (symbols.toml). the story flags (Flags_Test row)

// --- the master's cells --------------------------------------------------------------
// kMaster: now AreaW0c_TailSub (symbols.toml). the master spoken to (the per-master message table's index)
constexpr ::bof3::image::Va kMetFlags{0x904654};    // Flags_Set by master when the introduction opens
constexpr ::bof3::image::Va kIntroFlags{0x904657};  // Flags_Test / Flags_Set by master: the introduction done
constexpr ::bof3::image::Va kMasterMessages{0x6644E8};  // u16 per master: its script messages' base
// kTalkMode: now AreaW0c_AnswerMark (symbols.toml). MasterTalk_States' index
// kTalkStep: now AreaW3f_AnswerMarkD1 (symbols.toml). its step
// kTalkSlide: now Rest2c_TalkSlide (symbols.toml). the panels' slide counter (4 out .. 0 in)
// kTalkPick: now Rest2d_Cursor (symbols.toml). the pick (cleared when the panels are in)
// kTalkD0: now Rest2c_TalkD0 (symbols.toml). cleared by MasterTalk_Reset
// kTalkCD: now Rest2c_TalkCD (symbols.toml). cleared by MasterTalk_Reset
constexpr ::bof3::image::Va kMembers{0x802E88};     // ObjTrio + 0x148: Field_Members' record index, 0x14C apart
constexpr std::uint32_t kObjStride = 0x14C;
constexpr ::bof3::image::Va kExpTable{0x658F48};    // Char_ExpTable: 0x318 bytes a member, a u16 every 8
constexpr ::bof3::image::Va kFaceTable{0x6644B8};   // 4 bytes an id: u, v, the CLUT's x << 4, its row
constexpr ::bof3::image::Va kStatLabels{0x66A0F8};  // DIV-0064's four menu stat labels, 8 apart
constexpr ::bof3::image::Va kStatusA{0x66A0E8};     // DIV-0064's status words
constexpr ::bof3::image::Va kStatusB{0x66A0F0};
constexpr ::bof3::image::Va kFmtNumber{0x64E324};   // a number's format
constexpr ::bof3::image::Va kFmtLeft{0x6639B0};     // a figure's (the current)
constexpr ::bof3::image::Va kFmtRight{0x6639A8};    // and its maximum's
constexpr ::bof3::image::Va kStatsPieces{0x664468}; // Menu_DrawPieces lists: the stats box
constexpr ::bof3::image::Va kStatsRow{0x664494};    // its row, sixteen times 8 apart
constexpr ::bof3::image::Va kStatsEnd{0x6644A0};
constexpr ::bof3::image::Va kMemberPieces{0x6643D8}; // the member panel's
constexpr ::bof3::image::Va kFrameCounter{0x937F94}; // Frame_Counter (bit 5: the status words' blink)
constexpr ::bof3::image::Va kPacketNext{0x7E0670};  // Gfx_PacketNext
constexpr rdata::Const kBoxAdd{0x5C41C0};      // the panel box's three float constants (read in place)
constexpr rdata::Const kBoxSub{0x5C41BC};
constexpr rdata::Const kBoxSubBottom{0x5C41B8};

// --- model A (0x9398E0, dispatched by R2B's 0x57F320 on +1), and what follows it ----
// R2B's layout (docs/rest_2c.md 1.1): model A 0x80 bytes, model B 0x939960, then
// the screen's cells 0x9399E0.. - the cells past +0x80 below are not A's.
constexpr ::bof3::image::Va kFigure{0x9398E0};
// kFigureDone: now Rest2c_FigureDone (symbols.toml). +6: 1 when a move reached its end
// kFigureY: now Rest2c_FigureY (symbols.toml). +0x3C: the height, 16.16
// kFigureScale: now Rest2c_FigureScale (symbols.toml). +0x40
constexpr ::bof3::image::Va kFigureRgb{0x93993D};   // +0x5D..+0x5F: the colour
// kFigureAngle: now Rest2c_FigureAngle (symbols.toml). +0x6C
// kFigureLift: now Rest2c_FigureLift (symbols.toml). model B's scale (+0x40): A stands on B, in 0xA00ths
// kFigureFade: now Rest2c_FigureFade (symbols.toml). the fourth "given" count (R2B's 0x9399E4 + 3): the fade's step
// kFigureScaleIndex: now Rest2c_FigureScaleIndex (symbols.toml). R2B's level (Shisu_ScaleIndex's answer)
constexpr ::bof3::image::Va kScaleTable{0x663D7C};  // the scales the index reads (in place)
// kKind2X: now Field_Kind2X (symbols.toml). Field_Kind2X
// kKind2Z: now Field_Kind2Z (symbols.toml). Field_Kind2Z

// --- the windows (WindowRecords 0x803160, 0x24 each) ---------------------------------
// kWindows: now WindowRecords (symbols.toml).
constexpr std::uint32_t kWindowStride = 0x24;

// --- callees another group of this wave owns, and the C runtime's ------------------
constexpr ::bof3::image::CodeVa kFigureDraw{bof3::addr::Shisu_DrawModel};   // R2B: the figure record drawn (one word: the record)
constexpr ::bof3::image::CodeVa kPickAsk{bof3::addr::MasterScreen_PickMember};      // R2D: the master's pick, (message u16, a byte)
constexpr ::bof3::image::CodeVa kGlyph{bof3::addr::BattleEquipWin_DrawBar};        // R2H: an 8 x 8 cell (x, y, u / 8, v / 8, clut, shade)

}  // namespace at
}  // namespace rest_2c
