// Group R2H's raw addresses (round fourteen, wave two; docs/rest_2h.md): the
// .data step tables its window kinds dispatch through (each named as a [[data]]
// entry in symbols.toml with the count read by hand, section 3 of the doc), the
// cells and the .data the group's code names by address, the two call sites
// language and frame divergences re-aim, and the two callees that are nobody's
// by address (the C runtime's _stricmp; nothing of a later wave is called).
#pragma once

#include <cstdint>

namespace rest_2h {
namespace at {

// --- the step tables (the window record's +3 indexes each, unchecked) --------
constexpr std::uint32_t kGeneSteps = 0x66B1CC;            // 3  MenuList_GeneWinRun
constexpr std::uint32_t kSharedListSteps = 0x66B2D8;      // 5  ShopWin_SharedListRun
constexpr std::uint32_t kSharedMemberSteps = 0x66B2EC;    // 5  ShopWin_SharedMemberRun
constexpr std::uint32_t kMemberStatusSteps = 0x66B300;    // 3  ShopWin_MemberStatusRun
constexpr std::uint32_t kMasterListSteps = 0x66B384;      // 4  ShopWin_MasterListRun
constexpr std::uint32_t kMasterCaptionSteps = 0x66B394;   // 3  ShopWin_MasterCaptionRun
constexpr std::uint32_t kPupilsSteps = 0x66B3A0;          // 3  ShopWin_PupilsRun
constexpr std::uint32_t kItemCountSteps = 0x66B3AC;       // 3  ShopWin_ItemCountRun
constexpr std::uint32_t kEquipSteps = 0x66B56C;           // 4  BattleMenuWin_EquipRun
constexpr std::uint32_t kEquipItemsSteps = 0x66B57C;      // 4  BattleMenuWin_EquipItemsRun

// --- the window records ----------------------------------------------------------
constexpr std::uint32_t kCurrent = 0x905B84;       // unsigned char *: the record Field_RunTaskRecords is on
constexpr std::uint32_t kWindows = 0x803160;       // WindowRecords, 22 of 0x24
constexpr std::uint32_t kWindowStride = 0x24;
constexpr std::uint32_t kWindow1Master = 0x803191; // window 1's +0xD: the master list's chosen entry
constexpr std::uint32_t kStyle = 0x903A5A;         // the window colour byte Menu_DrawBox is handed

// --- the character records and the party ------------------------------------------
constexpr std::uint32_t kRecords = 0x903A70;       // CharacterRecords, 0xA4 each
constexpr std::uint32_t kRecordStride = 0xA4;
constexpr std::uint32_t kMemberMap = 0x66972C;     // a member id's record (MoveScript_EffectState's bytes), unbounded
constexpr std::uint32_t kPartyBits = 0x904061;     // the byte MasterWin_Available tests (bits 3..5)
constexpr std::uint32_t kStoryFlags = 0x904030;    // Cond_Flags + 0xA0
constexpr std::uint32_t kBattleParty = 0x904065;   // the second party list (three bytes)
constexpr std::uint32_t kSharedList = 0x904574;    // the shared ability list, 128 bytes
constexpr std::uint32_t kSkillSlots = 0x903AEE;    // a record's ten ability slots (+0x7E)

// --- the gene list (MenuList kind 20) -------------------------------------------------
constexpr std::uint32_t kGenes = 0x904650;         // u32: the eighteen gene bits
constexpr std::uint32_t kGeneList = 0x6BE0B0;      // 18 bytes: bit + 1 of each set bit, then 0
constexpr std::uint32_t kGeneTitle = 0x66B1F0;     // the list's title
constexpr std::uint32_t kGenePiecesA = 0x66B1D8;
constexpr std::uint32_t kGenePiecesB = 0x66B1E4;

// --- the row menu (handler 7 kind 14) ---------------------------------------------------
constexpr std::uint32_t kRowText = 0x66B36C;       // four text pointers: the title, then the rows' by 0x66B37D
constexpr std::uint32_t kRowCount = 0x66B37C;      // u8: the rows
constexpr std::uint32_t kRowOrder = 0x66B37D;      // u8 each: a row's text index
constexpr std::uint32_t kRowPiecesA = 0x66B30C;
constexpr std::uint32_t kRowPiecesB = 0x66B330;
constexpr std::uint32_t kRowPiecesC = 0x66B33C;

// --- the masters (handler 7 kinds 15..17) ---------------------------------------------
constexpr std::uint32_t kMasterBits = 0x904657;    // 17 bits: the masters met
constexpr std::uint32_t kMasterTitle = 0x66A1F0;   // the list's title (yes-no-prompts.md section 5)
constexpr std::uint32_t kMasterMark = 0x66A2D8;    // the text drawn beside an available master
constexpr std::uint32_t kMasterPiecesA = 0x66B3B8;
constexpr std::uint32_t kMasterPiecesB = 0x66B3C4;
constexpr std::uint32_t kRequirements = 0x66B424;  // 17 pointers: each master's skill list, 0xFF-ended
constexpr unsigned kMasterCount = 17;
constexpr std::uint32_t kPupilLabel = 0x66A1F8;    // the portrait box's label (yes-no-prompts.md section 5)
constexpr std::uint32_t kPortraitCells = 0x66B470; // 4 bytes a portrait: u, v, the CLUT's x, y (unbounded)
// MasterWin_SlideOut's bound: the imm32 of its `mov ecx, imm32` (0xB9 at
// 0x59C135), which Widescreen_Inject (DIV-0041) moves out by its columns.
constexpr std::uint32_t kMasterBoundMov = 0x59C135;
constexpr std::uint32_t kMasterBound = 0x59C136;

// --- the battle equipment item list (handler 8 kind 4) ----------------------------------
constexpr std::uint32_t kIdLists = 0x656B00;       // Inventory_IdLists, by category
constexpr std::uint32_t kCountLists = 0x656B14;    // Inventory_CountLists, by category
constexpr std::uint32_t kUseIds = 0x6BE0C4;        // 128 bytes: the items listed
constexpr std::uint32_t kUseCounts = 0x6BE144;     // 128 bytes: their counts
constexpr std::uint32_t kUseTitles = 0x66B5C4;     // the category titles' pointers (labels.cpp retargets them)
constexpr std::uint32_t kUsePiecesA = 0x66B4B0;
constexpr std::uint32_t kUsePiecesB = 0x66B4BC;
constexpr std::uint32_t kFmtCount = 0x6639C8;      // "n / total"

// --- the verb pair (handler 8 kind 5) -------------------------------------------------
constexpr std::uint32_t kVerbA = 0x66A228;         // Menu_Verbs' entries 0 and 3
constexpr std::uint32_t kVerbB = 0x66A240;
constexpr std::uint32_t kVerbPiecesOn = 0x66B500;
constexpr std::uint32_t kVerbPiecesOff = 0x66B4F8;

// --- text --------------------------------------------------------------------------------
constexpr std::uint32_t kPrintBuf = 0x904BA0;      // the text scratch Crt_sprintf writes
constexpr std::uint32_t kFmtLevel = 0x64E324;
constexpr std::uint32_t kFmtFigureA = 0x6639B0;
constexpr std::uint32_t kFmtFigureB = 0x6639A8;
constexpr std::uint32_t kStatusA = 0x66A0E8;
constexpr std::uint32_t kStatusB = 0x66A0F0;

// --- the re-aimed call sites ------------------------------------------------------------
// DIV-0011: MenuList_ReserveWinDraw's first call (E8 rel32), re-aimed by
// menu_frame.cpp at Menu_DrawFrame; the original's callee is the empty 0x4DF820.
constexpr std::uint32_t kFrameSite = 0x59AA98;
// DIV-0059: BattleEquipWin_DrawItems' title draw (E8 rel32), re-aimed by
// battle_draw.cpp at ListTitle_DrawAt under a Latin overlay; else Text_DrawAt.
constexpr std::uint32_t kTitleSite = 0x59DEFA;

// --- the joystick enumeration (DInput_EnumJoystick) --------------------------------------
constexpr std::uint32_t kJoystickIid = 0x5C4718;   // the interface QueryInterface asks for
constexpr std::uint32_t kProductName = 0x66C7B0;   // the product name compared, case-blind
// The C runtime's _stricmp (catalogue part 0, nobody's): (a, b) -> 0 when equal.
constexpr std::uint32_t kStricmp = 0x5C2B40;

}  // namespace at
}  // namespace rest_2h
