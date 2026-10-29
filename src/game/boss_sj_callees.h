// Internal to boss_sj.cpp and boss_sj_fuzz.cpp: the cells the 44 functions of
// group BSJ touch that symbols.toml has no name for, and the callees nobody
// owns, by raw address. docs/boss_sj.md.
//
// Raw-address callees (the round's rebinding pass names them):
//   0x446DE0 / 0x446E00 / 0x446E20  (): the end phase's steps 1 (the win), 2
//             and 3 (boss_h_callees.h; the harness's standard set).
//   0x441090  (value, sign): the high word of the 16.16 value, one more when
//             its low word is not 0 and `sign` is not negative - a round-up
//             (area_w4f_callees.h: engine code nobody owns, 0x441090..0x4410AD;
//             read again 2026-09-28). Only ax is read by the callers here.
#pragma once

#include <cstdint>

#include "bof3/symbols.gen.h"


// Rebound 2026-09-29 (round twelve group BE4, docs/battle_e4.md section 9): the constants here naming BE4's functions read
// bof3::addr::<Name>; the values are unchanged (the fuzz keys on them).
namespace boss_sj {
namespace at {

using U = std::uint32_t;

// --- the battle's cells (0x904AA0..0x904BA0) ---------------------------------
constexpr ::bof3::image::Va kFlags{0x904AA8};            // u16: the round flags (bit 2 read by D>Lord's effect)
// kBattleEnd: now Battle_EndFlags (symbols.toml). u8: bit 1 the win
constexpr ::bof3::image::Va kActor{0x904B34};            // u8: the acting actor (3..10 an enemy)
// kPoseIndex: now BossSa_EndCount (symbols.toml). u16: kind 62's pose index (0x440630 / 0x4405E0 write it; 0x437C10 uses it as a count)

// --- the hooks ------------------------------------------------------------------
// kHookEnd: now BattleHook_End (symbols.toml). BattleHook_End
// kHookExit: now BattleHook_Exit (symbols.toml). BattleHook_Exit
// kHookEvent: now BattleHook_Event (symbols.toml). BattleHook_Event

// --- the enemies and the task slots ----------------------------------------------
// kCurrentEnemy: now BattleE3_CurrentEnemy (symbols.toml). the enemy BattleEnemy_RunAll is running
// kEnemies: now Battle_Enemies (symbols.toml). enemy objects, stride 0x128
constexpr U kEnemyStride = 0x128;
constexpr ::bof3::image::Va kEnemy0Type{0x93BA50};       // enemy 0's +0xF0 (its data record's index; Battle_CopyEnemyData)
// kTasks: now BattleTask_Slots (symbols.toml). BattleTask_Create's 48 slots of 0x84
constexpr U kTaskStride = 0x84;
constexpr unsigned kTaskCount = 48;
// kOwner: now BattleE2_Owner (symbols.toml). unsigned char *: the running task's owner (BattleTask_RunAll)

// The area's enemy data records (stride 0x8C): +0x8A is the effect size byte
// (BattleActor_FxSize reads the same cell).
constexpr ::bof3::image::Va kEnemyFxSize{0x8C5652};      // 0x8C55C8 + 0x8A
constexpr U kEnemyDataStride = 0x8C;

// --- the party (ObjTrio, stride 0x14C) -----------------------------------------
// kParty: now ObjTrio (symbols.toml).
constexpr U kPartyStride = 0x14C;

// --- other cells ------------------------------------------------------------
constexpr ::bof3::image::Va kScriptVar3{0x903848};       // u8: movement-script variable 3 (area_w0a.md), set by set-up 54's end
// kChapterStep: now MoveScript_PartyRecords[0x65] (symbols.toml). u8: the chapter run's step
constexpr ::bof3::image::Va kPoseSet{0x8C5D80};          // the frame set the end hooks pose the party from

// --- the callees nobody owns ------------------------------------------------
constexpr ::bof3::image::CodeVa kEndWin{bof3::addr::BattleEnd_EnterStep1};           // () the end phase, step 1
constexpr ::bof3::image::CodeVa kEndOther{bof3::addr::BattleEnd_EnterStep2};         // () step 2
constexpr ::bof3::image::CodeVa kEndThird{bof3::addr::BattleEnd_EnterStep3};         // () step 3
constexpr ::bof3::image::CodeVa kRoundHigh{bof3::addr::Fixed_HighRoundUp};        // (value, sign) -> ax (BE3's since round twelve: the same value)

}  // namespace at
}  // namespace boss_sj
