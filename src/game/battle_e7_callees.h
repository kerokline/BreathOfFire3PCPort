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
constexpr std::uint32_t kStatGain = bof3::addr::Char_LevelUpGain;
// 0x59DB70 (in no group: catalogue part 7): (x, y, colour, width, clut,
// shade) - a SPRT8 strip, 0x59D640's preview bars. Read as words and bytes.
constexpr std::uint32_t kDrawBar = 0x59DB70;

// --- the slide-out bounds DIV-0041 widens -----------------------------------
// The imm16 of each list's `cmp ax, imm16` (66 3D before it) in the
// original's code, which Widescreen_Inject (widescreen.cpp kSlides) moves
// outward by its columns: ours reads the bound there, so the widened one
// survives the takeover and BOF3X_ORIGINAL=Widescreen still restores these.
constexpr std::uint32_t kListOutBound = 0x599061;    // -0xA5 in GeneWin_ListSlideOut 0x599050
constexpr std::uint32_t kList2OutBound = 0x599451;   // 0x15B in GeneWin_List2SlideOut 0x599440
constexpr std::uint32_t kList3OutBound = 0x599551;   // 0x143 in GeneWin_List3SlideOut 0x599540

// --- the cells (every one an address in BOF3.exe) ---------------------------
constexpr std::uint32_t kWindowCurrent = 0x905B84;   // the window record a handler runs for
constexpr std::uint32_t kColour = 0x903A5A;          // s8: the window colour, a CLUT row
constexpr std::uint32_t kClutShadow = 0x80B7A8;      // the CLUT shadow rows (64 bytes each)
constexpr std::uint32_t kTasks = 0x93A000;           // BattleTask_Create's 48 slots of 0x84
constexpr std::uint32_t kTaskStride = 0x84;
constexpr unsigned kTaskCount = 48;
constexpr std::uint32_t kMenuActor = 0x939EC4;       // the command menu's member (an ObjTrio record)
constexpr std::uint32_t kPartyAp = 0x802DDA;         // ObjTrio +0x9A (AP), stride 0x14C
constexpr std::uint32_t kPartyStride = 0x14C;
constexpr std::uint32_t kCharRecords = 0x903A70;     // CharacterRecords, stride 0xA4
constexpr std::uint32_t kCharStride = 0xA4;
constexpr std::uint32_t kPrintBuf = 0x904BA0;        // the battle windows' sprintf buffer
constexpr std::uint32_t kLevelBufs = 0x904D20;       // the level-up window's eight buffers of 0x20
constexpr std::uint32_t kStep2 = 0x904AA3;           // the battle's step bytes
constexpr std::uint32_t kStep3 = 0x904AA4;
constexpr std::uint32_t kGeneChoice = 0x904AA5;      // bit 7 fixed, bits 0..6 the choice (0..2)
constexpr std::uint32_t kGeneRow = 0x904AA6;         // 0xFF: the hand on the first box
constexpr std::uint32_t kGeneColumn = 0x904AA7;
constexpr std::uint32_t kApCost = 0x904B78;          // u8: the AP cost (battle_actions.md)
constexpr std::uint32_t kDropCount = 0x904AE7;       // u8: the drops listed
constexpr std::uint32_t kDropItems = 0x904AF4;       // u16 x n: category << 8 | item
constexpr std::uint32_t kDropCounts = 0x904B14;      // u8 x n
constexpr std::uint32_t kGeneList = 0x904608;        // 4-byte rows: three cost indices (0xFF none), a form byte
constexpr std::uint32_t kGeneList2 = 0x904620;       // the second list's rows (12)
constexpr std::uint32_t kScrollMarks = 0x939848;     // 6 bytes: the first list's form bytes + 1
constexpr std::uint32_t kScrollMarks2 = 0x939850;    // 12 bytes: the second list's
constexpr std::uint32_t kCostTable = 0x64EC9C;       // u8 by cost index
constexpr std::uint32_t kFormLabels = 0x64ECB0;      // u8 message id by form (& 0x1F)
constexpr std::uint32_t kFormCells = 0x66AF70;       // u8 icon cell by form
constexpr std::uint32_t kTitles = 0x66AF64;          // 5-byte strings by title index
constexpr std::uint32_t kChoiceTexts = 0x66A164;     // three text pointers
constexpr std::uint32_t kIconKinds = 0x66AF24;       // u8 icon by Item_IconKind
constexpr std::uint32_t kLevelTable = 0x658F48;      // Char_ExpTable: 99 rows of 8 bytes per character
constexpr std::uint32_t kAbilityRecords = 0x65C4C8;  // Ability_Records, stride 24
constexpr std::uint32_t kEquipCategories = 0x66B5BC; // u8 x 6: the equipment slots' item categories
constexpr std::uint32_t kEquipIcons = 0x66B5B4;      // u8 x 6
constexpr std::uint32_t kFieldMember = 0x929F06;     // s8: BATE's member (0x904065's index)
constexpr std::uint32_t kPartyOrder = 0x904065;      // u8 x n: the party's members
constexpr std::uint32_t kMemberIds = 0x66972C;       // MoveScript_EffectState: a member byte by slot

// --- .data strings and piece lists (addresses only) -------------------------
constexpr std::uint32_t kFmtNumber = 0x5E10C0;       // Area08_MessageFormat
constexpr std::uint32_t kFmtAbility = 0x654900;
constexpr std::uint32_t kFmtCount = 0x653EC0;
constexpr std::uint32_t kFmtAp = 0x64D3EC;           // Boss26Fx_CountFormat
constexpr std::uint32_t kFmtCost = 0x66AF8C;
constexpr std::uint32_t kFmtStat = 0x64E324;
constexpr std::uint32_t kApLabel = 0x66AF38;
constexpr std::uint32_t kChoicePieces = 0x66AF3C;
constexpr std::uint32_t kChoicePiecesOn = 0x66AF44;
constexpr std::uint32_t kListPieces = 0x66AF4C;
constexpr std::uint32_t kListPieces2 = 0x66AF58;
constexpr std::uint32_t kEquipPieces = 0x66B508;
constexpr std::uint32_t kStatLabels[4] = {0x66A0F8, 0x66A100, 0x66A108, 0x66A110};

}  // namespace at
}  // namespace battle_e7
