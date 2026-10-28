// Group R2H's raw addresses (round fourteen, wave two; docs/rest_2h.md): the
// .data step tables its window kinds dispatch through (each named as a [[data]]
// entry in symbols.toml with the count read by hand, section 3 of the doc), the
// cells and the .data the group's code names by address, the two call sites
// language and frame divergences re-aim, and the two callees that are nobody's
// by address (the C runtime's _stricmp; nothing of a later wave is called).
#pragma once

#include <cstdint>

#include "game/rdata_consts.h"

namespace rest_2h {
namespace at {

// --- the step tables (the window record's +3 indexes each, unchecked) --------
// kGeneSteps: now MenuList_GeneWinSteps (symbols.toml). 3  MenuList_GeneWinRun
// kSharedListSteps: now ShopWin_SharedListSteps (symbols.toml). 5  ShopWin_SharedListRun
// kSharedMemberSteps: now ShopWin_SharedMemberSteps (symbols.toml). 5  ShopWin_SharedMemberRun
// kMemberStatusSteps: now ShopWin_MemberStatusSteps (symbols.toml). 3  ShopWin_MemberStatusRun
// kMasterListSteps: now ShopWin_MasterListSteps (symbols.toml). 4  ShopWin_MasterListRun
// kMasterCaptionSteps: now ShopWin_MasterCaptionSteps (symbols.toml). 3  ShopWin_MasterCaptionRun
// kPupilsSteps: now ShopWin_PupilsSteps (symbols.toml). 3  ShopWin_PupilsRun
// kItemCountSteps: now ShopWin_ItemCountSteps (symbols.toml). 3  ShopWin_ItemCountRun
// kEquipSteps: now BattleMenuWin_EquipSteps (symbols.toml). 4  BattleMenuWin_EquipRun
// kEquipItemsSteps: now BattleMenuWin_EquipItemsSteps (symbols.toml). 4  BattleMenuWin_EquipItemsRun

// --- the window records ----------------------------------------------------------
// kCurrent: now BattleWinStates_Current (symbols.toml). unsigned char *: the record Field_RunTaskRecords is on
// kWindows: now WindowRecords (symbols.toml). WindowRecords, 22 of 0x24
constexpr std::uint32_t kWindowStride = 0x24;
// kWindow1Master: now Window1_Master (symbols.toml). window 1's +0xD: the master list's chosen entry
// kStyle: now Config_WindowColour (symbols.toml). the window colour byte Menu_DrawBox is handed

// --- the character records and the party ------------------------------------------
// kRecords: now CharacterRecords (symbols.toml). CharacterRecords, 0xA4 each
constexpr std::uint32_t kRecordStride = 0xA4;
constexpr ::bof3::image::Va kMemberMap{0x66972C};     // a member id's record (Character_IndexById's bytes), unbounded
// kPartyBits: now Cond_Flags[0xd1] (symbols.toml). the byte MasterWin_Available tests (bits 3..5)
// kStoryFlags: now Cond_Flags[0xa0] (symbols.toml). Cond_Flags + 0xA0
constexpr ::bof3::image::Va kBattleParty{0x904065};   // the second party list (three bytes)
constexpr ::bof3::image::Va kSharedList{0x904574};    // the shared ability list, 128 bytes
// kSkillSlots: now Field_ActorStates[0x6e] (symbols.toml). a record's ten ability slots (+0x7E)

// --- the gene list (MenuList kind 20) -------------------------------------------------
// kGenes: now Gene_Bits (symbols.toml). u32: the eighteen gene bits
constexpr ::bof3::image::Va kGeneList{0x6BE0B0};      // 18 bytes: bit + 1 of each set bit, then 0
constexpr ::bof3::image::Va kGeneTitle{0x66B1F0};     // the list's title
constexpr ::bof3::image::Va kGenePiecesA{0x66B1D8};
constexpr ::bof3::image::Va kGenePiecesB{0x66B1E4};

// --- the row menu (handler 7 kind 14) ---------------------------------------------------
constexpr ::bof3::image::Va kRowText{0x66B36C};       // four text pointers: the title, then the rows' by 0x66B37D
// kRowCount: now Rest2h_RowCount (symbols.toml). u8: the rows
constexpr ::bof3::image::Va kRowOrder{0x66B37D};      // u8 each: a row's text index
constexpr ::bof3::image::Va kRowPiecesA{0x66B30C};
constexpr ::bof3::image::Va kRowPiecesB{0x66B330};
constexpr ::bof3::image::Va kRowPiecesC{0x66B33C};

// --- the masters (handler 7 kinds 15..17) ---------------------------------------------
constexpr ::bof3::image::Va kMasterBits{0x904657};    // 17 bits: the masters met
constexpr ::bof3::image::Va kMasterTitle{0x66A1F0};   // the list's title (yes-no-prompts.md section 5)
constexpr ::bof3::image::Va kMasterMark{0x66A2D8};    // the text drawn beside an available master
constexpr ::bof3::image::Va kMasterPiecesA{0x66B3B8};
constexpr ::bof3::image::Va kMasterPiecesB{0x66B3C4};
constexpr ::bof3::image::Va kRequirements{0x66B424};  // 17 pointers: each master's skill list, 0xFF-ended
constexpr unsigned kMasterCount = 17;
constexpr ::bof3::image::Va kPupilLabel{0x66A1F8};    // the portrait box's label (yes-no-prompts.md section 5)
constexpr ::bof3::image::Va kPortraitCells{0x66B470}; // 4 bytes a portrait: u, v, the CLUT's x, y (unbounded)
// MasterWin_SlideOut's bound: the imm32 of its `mov ecx, imm32` (0xB9 at
// 0x59C135), which Widescreen_Inject (DIV-0041) moves out by its columns.
constexpr std::uint32_t kMasterBoundMov = 0x59C135;
constexpr std::uint32_t kMasterBound = 0x59C136;

// --- the battle equipment item list (handler 8 kind 4) ----------------------------------
constexpr ::bof3::image::Va kIdLists{0x656B00};       // Inventory_IdLists, by category
constexpr ::bof3::image::Va kCountLists{0x656B14};    // Inventory_CountLists, by category
constexpr ::bof3::image::Va kUseIds{0x6BE0C4};        // 128 bytes: the items listed
constexpr ::bof3::image::Va kUseCounts{0x6BE144};     // 128 bytes: their counts
constexpr ::bof3::image::Va kUseTitles{0x66B5C4};     // the category titles' pointers (labels.cpp retargets them)
constexpr ::bof3::image::Va kUsePiecesA{0x66B4B0};
constexpr ::bof3::image::Va kUsePiecesB{0x66B4BC};
constexpr ::bof3::image::Va kFmtCount{0x6639C8};      // "n / total"

// --- the verb pair (handler 8 kind 5) -------------------------------------------------
// kVerbA: now Menu_Verbs (symbols.toml). Menu_Verbs' entries 0 and 3
// kVerbB: now Menu_Verbs[0x18] (symbols.toml).
constexpr ::bof3::image::Va kVerbPiecesOn{0x66B500};
constexpr ::bof3::image::Va kVerbPiecesOff{0x66B4F8};

// --- text --------------------------------------------------------------------------------
constexpr ::bof3::image::Va kPrintBuf{0x904BA0};      // the text scratch Crt_sprintf writes
constexpr ::bof3::image::Va kFmtLevel{0x64E324};
constexpr ::bof3::image::Va kFmtFigureA{0x6639B0};
constexpr ::bof3::image::Va kFmtFigureB{0x6639A8};
constexpr ::bof3::image::Va kStatusA{0x66A0E8};
constexpr ::bof3::image::Va kStatusB{0x66A0F0};

// --- the re-aimed call sites ------------------------------------------------------------
// DIV-0011: MenuList_ReserveWinDraw's first call (E8 rel32), re-aimed by
// menu_frame.cpp at Menu_DrawFrame; the original's callee is the empty 0x4DF820.
constexpr std::uint32_t kFrameSite = 0x59AA98;
// DIV-0059: BattleEquipWin_DrawItems' title draw (E8 rel32), re-aimed by
// battle_draw.cpp at ListTitle_DrawAt under a Latin overlay; else Text_DrawAt.
constexpr std::uint32_t kTitleSite = 0x59DEFA;

// --- the joystick enumeration (DInput_EnumJoystick) --------------------------------------
constexpr rdata::Const kJoystickIid{0x5C4718};   // the interface QueryInterface asks for
constexpr ::bof3::image::Va kProductName{0x66C7B0};   // the product name compared, case-blind
// The C runtime's _stricmp (catalogue part 0, nobody's): (a, b) -> 0 when equal.
constexpr std::uint32_t kStricmp = 0x5C2B40;

}  // namespace at
}  // namespace rest_2h
