// Group BE7's callees nobody owns yet (round twelve, wave one): each by its
// raw address, called through the boss harness (BH_AT) so that the start-up
// fuzz reaches its recorder. docs/battle_e7.md section 6 lists the edges for
// the rebinding pass after the wave.
#pragma once

#include <cstdint>

#include "bof3/symbols.gen.h"   // the constants below name their functions since 2026-10-01 (round twelve's debt 2): the same values, so the fuzz keys stand

namespace battle_e7 {
namespace at {

// --- functions of another group of this wave, or of no group ---------------
// 0x432170 (BE1's, not merged): (member, stat 1..6) - a level's gain of one
// stat, answered in ax (0 none). 0x597FC0 calls it twice per stat.
constexpr ::bof3::image::CodeVa kStatGain{bof3::addr::Char_LevelUpGain};
// 0x59DB70 (in no group: catalogue part 7): (x, y, colour, width, clut,
// shade) - a SPRT8 strip, 0x59D640's preview bars. Read as words and bytes.
constexpr ::bof3::image::CodeVa kDrawBar{bof3::addr::BattleEquipWin_DrawBar};   // 0x59DB70, ours since round fourteen R2H

// --- the slide-out bounds of DIV-0041's faster slide ------------------------
// The imm16 of each list's `cmp ax, imm16` (66 3D before it) in the
// original's code: ours reads each as widescreen.cpp holds it (g_slides,
// Capcom's at every width); the wide one is only where the list is drawn
// (docs/ultrawide-slides.md).
constexpr ::bof3::image::CodeVa kListOutBound{0x599061};    // -0xA5 in GeneWin_ListSlideOut 0x599050
constexpr ::bof3::image::CodeVa kList2OutBound{0x599451};   // 0x15B in GeneWin_List2SlideOut 0x599440
constexpr ::bof3::image::CodeVa kList3OutBound{0x599551};   // 0x143 in GeneWin_List3SlideOut 0x599540

// --- the cells (every one an address in BOF3.exe) ---------------------------
// kWindowCurrent: now BattleWinStates_Current (symbols.toml). the window record a handler runs for
// kColour: now Config_WindowColour (symbols.toml). s8: the window colour, a CLUT row
// kClutShadow: now Gfx_ClutStripSource[0x114] (symbols.toml). the CLUT shadow rows (64 bytes each)
// kTasks: now BattleTask_Slots (symbols.toml). BattleTask_Create's 48 slots of 0x84
constexpr std::uint32_t kTaskStride = 0x84;
constexpr unsigned kTaskCount = 48;
// kMenuActor: now BattleE4_MenuActor (symbols.toml). the command menu's member (an ObjTrio record)
// kPartyAp: now ObjTrio[0].ap (symbols.toml). ObjTrio +0x9A (AP), stride 0x14C
constexpr std::uint32_t kPartyStride = 0x14C;
// kCharRecords: now CharacterRecords (symbols.toml). CharacterRecords, stride 0xA4
constexpr std::uint32_t kCharStride = 0xA4;
constexpr ::bof3::image::Va kPrintBuf{0x904BA0};        // the battle windows' sprintf buffer
constexpr ::bof3::image::Va kLevelBufs{0x904D20};       // the level-up window's eight buffers of 0x20
// kStep2: now BattleE4_Step3 (symbols.toml). the battle's step bytes
// kStep3: now BattleE4_Step4 (symbols.toml).
// kGeneChoice: now BossSb_Countdown (symbols.toml). bit 7 fixed, bits 0..6 the choice (0..2)
// kGeneRow: now BattleE5_GeneRow (symbols.toml). 0xFF: the hand on the first box
// kGeneColumn: now BattleE1_Roster (symbols.toml).
// kApCost: now BattleActions_ApCost (symbols.toml). u8: the AP cost (battle_actions.md)
// kDropCount: now Battle_DropCount (symbols.toml). u8: the drops listed
constexpr ::bof3::image::Va kDropItems{0x904AF4};       // u16 x n: category << 8 | item
constexpr ::bof3::image::Va kDropCounts{0x904B14};      // u8 x n
constexpr ::bof3::image::Va kGeneList{0x904608};        // 4-byte rows: three cost indices (0xFF none), a form byte
constexpr ::bof3::image::Va kGeneList2{0x904620};       // the second list's rows (12)
constexpr ::bof3::image::Va kScrollMarks{0x939848};     // 6 bytes: the first list's form bytes + 1
constexpr ::bof3::image::Va kScrollMarks2{0x939850};    // 12 bytes: the second list's
constexpr ::bof3::image::Va kCostTable{0x64EC9C};       // u8 by cost index
constexpr ::bof3::image::Va kFormLabels{0x64ECB0};      // u8 message id by form (& 0x1F)
constexpr ::bof3::image::Va kFormCells{0x66AF70};       // u8 icon cell by form
constexpr ::bof3::image::Va kTitles{0x66AF64};          // 5-byte strings by title index
constexpr ::bof3::image::Va kChoiceTexts{0x66A164};     // three text pointers
constexpr ::bof3::image::Va kIconKinds{0x66AF24};       // u8 icon by Item_IconKind
constexpr ::bof3::image::Va kLevelTable{0x658F48};      // Char_ExpTable: 99 rows of 8 bytes per character
constexpr ::bof3::image::Va kAbilityRecords{0x65C4C8};  // Ability_Records, stride 24
constexpr ::bof3::image::Va kEquipCategories{0x66B5BC}; // u8 x 6: the equipment slots' item categories
constexpr ::bof3::image::Va kEquipIcons{0x66B5B4};      // u8 x 6
// kFieldMember: now BattleActorCopies_ItemActor (symbols.toml). s8: BATE's member (0x904065's index)
constexpr ::bof3::image::Va kPartyOrder{0x904065};      // u8 x n: the party's members
// kMemberIds: now Character_IndexById (symbols.toml). Character_IndexById: a member byte by slot

// --- .data strings and piece lists (addresses only) -------------------------
constexpr ::bof3::image::Va kFmtNumber{0x5E10C0};       // Area08_MessageFormat
constexpr ::bof3::image::Va kFmtAbility{0x654900};
constexpr ::bof3::image::Va kFmtCount{0x653EC0};
// kFmtAp: now Boss26Fx_CountFormat (symbols.toml). Boss26Fx_CountFormat
constexpr ::bof3::image::Va kFmtCost{0x66AF8C};
constexpr ::bof3::image::Va kFmtStat{0x64E324};
constexpr ::bof3::image::Va kApLabel{0x66AF38};
constexpr ::bof3::image::Va kChoicePieces{0x66AF3C};
constexpr ::bof3::image::Va kChoicePiecesOn{0x66AF44};
constexpr ::bof3::image::Va kListPieces{0x66AF4C};
constexpr ::bof3::image::Va kListPieces2{0x66AF58};
constexpr ::bof3::image::Va kEquipPieces{0x66B508};
constexpr std::uint32_t kStatLabels[4] = {0x66A0F8, 0x66A100, 0x66A108, 0x66A110};

}  // namespace at
}  // namespace battle_e7
