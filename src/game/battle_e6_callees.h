// Internal to battle_e6.cpp and battle_e6_fuzz.cpp: the cells and .data tables
// group BE6's battle-engine code touches that symbols.toml has no name for,
// and the callees nobody owns, by raw address. docs/battle_e6.md.
//
// Raw-address callees (nobody owns them this round; the harness's engine set
// lists each, docs/boss_harness.md section 10.5):
//   0x446F20  (value, percent): value * percent / 100 clamped to 0..999 -
//             pure (the engine set's kThrough).
//   0x446F50  (value, percent): the same clamped to 0..9999.
//   0x446F80  (value, percent): the same clamped to 0..100.
//   0x4CF4B0  (x, z): a height for a BMAGIC cell's vertex (reads 0x6959CC,
//             0x695C2C / 0x695C2E; not read further); ax the answer.
// BE6 calls no function another group of this wave owns (tools/band_rows.py
// --edges, 2026-09-29).
#pragma once

#include <cstdint>

namespace battle_e6 {
namespace at {

using U = std::uint32_t;

// --- the battle bytes (0x904AA0..0x904BA0) ---------------------------------
constexpr U kStep2 = 0x904AA3;            // u8: a step byte (the gene history's step table by 0x904AA4 after it)
constexpr U kStep3 = 0x904AA4;            // u8
constexpr U kPartyCount = 0x904AB0;       // u8: the party's count (the transformation copies that many records)
constexpr U kPartySize = 0x904AB1;        // u8: the party's size as another byte (battle_menu_states.md M2); 3 lets a gene 0x10 form ask the others
constexpr U kFormTable = 0x904AAC;        // u8: the dash's side row (0x64E4F4 + 2 * it)
constexpr U kActor = 0x904B34;            // u8: the acting actor
constexpr U kActingSprite2 = 0x904B40;    // unsigned char *: the action record the roll reads (+2)
constexpr U kTarget = 0x904B44;           // u8: the target
constexpr U kGeneCost = 0x904B78;         // u8: the chosen genes' summed cost (battle_actions.md: the cost of ability 0x97)
constexpr U kFormGroup = 0x904B79;        // u8: the form's group code (0x675F57) kept
constexpr U kGenes = 0x904B84;            // u8 x 3: the chosen genes
constexpr U kGeneCount = 0x904B87;        // u8: how many
constexpr U kForm = 0x904B89;             // u8: the form code (0x675F56) kept; battle_obj_states.md reads it

// --- the gene history: six records of four bytes (three genes and the form
// byte (group << 5) + (form & 0x1F)), newest first ------------------------------
constexpr U kHistory = 0x904608;
constexpr U kHistoryForm = 0x90460B;
constexpr U kHistoryLast = 0x90461C;      // record 5

// --- the transformation's work cells ---------------------------------------
constexpr U kMixSums = 0x675F48;          // s8 x 14: the chosen genes' summed rows (0x675F48 .. 0x675F55)
constexpr U kFormCode = 0x675F56;         // u8: the form code
constexpr U kFormGroupWork = 0x675F57;    // u8: its group code
constexpr U kPartyBackup = 0x939AE0;      // the party records copied before the change (0x904AB0 x 0x14C bytes)
constexpr U kStatDeltas = 0x939EE0;       // u16 x 5 then u8 x 9: the new stats and their changes (0x939EE0 .. 0x939EF2)

// --- the gene history's windows (records 18 and 19 of WindowRecords, their +3)
constexpr U kWindow18State = 0x8033EB;
constexpr U kWindow19State = 0x80340F;

// --- the character records: 0x903A70 + 0xA4 * a member's +0x148 ------------
constexpr U kCharRecords = 0x903A70;
constexpr U kCharStride = 0xA4;

// --- the party (ObjTrio, 0x802D40 + 0x14C n) and the enemies' objects --------
constexpr U kParty = 0x802D40;
constexpr U kPartyStride = 0x14C;
constexpr U kEnemies = 0x93B960;
constexpr U kEnemyStride = 0x128;

// --- the battle tasks (48 of 0x84 at 0x93A000) and the running slot's owner ---
constexpr U kTasks = 0x93A000;
constexpr U kTaskStride = 0x84;
constexpr U kOwner = 0x93B940;

// --- the action flags set the roll tests (bit n of the dwords at 0x904088) ---
constexpr U kActionBits = 0x904088;
// the field's effect state bytes (MoveScript_EffectState, 24): a member's
// +0x89 indexes it; 6 picks the roll's second table
constexpr U kEffectState = 0x66972C;
// the action records' byte the roll reads (0x65C4D9 + 0x18 * id), its high nibble
constexpr U kActionRows = 0x65C4D9;
constexpr U kActionRowStride = 0x18;

// --- the kind-2 field cells the dash's rise reads (Field_Kind2X / _Kind2Z) ---
constexpr U kKind2Z = 0x905E60;
constexpr U kKind2X = 0x905E64;

// --- the .data tables (read in place; their contents are the exe's) ---------
constexpr U kGeneCosts = 0x64EC9C;        // u8 by gene: the cost DragonGenes_SumCost adds
constexpr U kHistorySteps = 0x64ED50;     // the history's step table (0x450F30 by 0x904AA4): 7 entries, DragonHistory_Leave the last
constexpr U kRecipeGroups = 0x64ED6C;     // u8 by recipe 0..10: the group code a found recipe sets
constexpr U kRecipeGenes = 0x64ED78;      // u8 x 3 by recipe 0..10: the genes a recipe needs (0xFF: any)
constexpr U kGeneRows = 0x64ED9C;         // s8 x 14 by gene: what a gene adds to the sums
constexpr U kShiftColumns = 0x64EE98;     // u8 x 5 by form code: added to the shifted stats
constexpr U kShiftSteps = 0x64EEAC;       // s8 x 5: DragonForm_StatShift's steps (by a sum + 2, 0..4)
constexpr U kMixBytesA = 0x64EEB4;        // u8 x 5 (SetMixBytes' first three)
constexpr U kMixBytesB = 0x64EEB9;        // u8 x 5 (its fourth)
constexpr U kMixBytesC = 0x64EEBE;        // u8 x 5 (its fifth)
constexpr U kFormAbilities = 0x64EEC4;    // u8 x 3 by form code 0..3: the mixed form's first abilities
constexpr U kAbilityRows = 0x64EED0;      // u8 x 3 by row 0..14: abilities a sum adds
constexpr U kRecipeStats = 0x64EF00;      // u8 x 14 by recipe 0..20: five stats' percents, nine bytes
constexpr U kRecipeAbilities = 0x64F028;  // u8 x 8 by recipe 0..20: its abilities
constexpr U kDashSteps = 0x64F0D0;        // BattleFxDash_Steps: 7 handlers by +1
constexpr U kPoseSteps = 0x64F0EC;        // BattleFxPose_Steps: 2
constexpr U kTrailSteps = 0x64F0F4;       // BattleFxTrail_Steps: 2 (the second BossWeretigrFx_TrailFade)
constexpr U kRollOdds = 0x64F0FC;         // u8 by nibble: the roll's divisor
constexpr U kRollOddsB = 0x64F104;        // u8 by nibble: the same when the member's effect state is 6
constexpr U kDashSides = 0x64E4F4;        // s8 pairs by 0x904AAC: x and z offsets for the dash's rise

// --- the BMAGIC cells' screen bounds (floats in .rdata) ----------------------
constexpr U kScreenYHi = 0x5C41FC;
constexpr U kScreenYLo = 0x5C4200;
constexpr U kScreenXHi = 0x5C4204;
constexpr U kScreenXLo = 0x5C4208;
constexpr U kSpriteXHi = 0x5C420C;
constexpr U kSpriteXLo = 0x5C4210;

// --- named cells whose symbols.gen.h names are object macros (the address
// constants' names expand): Field_Slots, Prim_VertexScratch,
// MapView_ScreenXY, Camera_Matrix, Draw_OtSlot
constexpr U kFieldSlots = 0x9035C0;       // Field_Slots: 8 of 0x10
constexpr U kVertexScratch = 0x9037A0;    // Prim_VertexScratch
constexpr U kScreenXY = 0x903820;         // MapView_ScreenXY (two floats)
constexpr U kCameraMatrix = 0x905E40;     // Camera_Matrix
constexpr U kOtSlot = 0x92BF19;           // Draw_OtSlot

// --- the callees nobody owns -----------------------------------------------
constexpr U kPercent999 = bof3::addr::Stat_PercentCap999;   // 0x446F20, R3B's (round fourteen; the same value): (value, percent)
constexpr U kPercent9999 = bof3::addr::Stat_PercentCap9999;   // 0x446F50, R3B's (round fourteen; the same value): (value, percent)
constexpr U kPercent100 = bof3::addr::Stat_PercentCap100;   // 0x446F80, R3B's (round fourteen; the same value): (value, percent)
constexpr U kCellHeight = 0x4CF4B0;       // (x, z): ax

}  // namespace at
}  // namespace battle_e6
