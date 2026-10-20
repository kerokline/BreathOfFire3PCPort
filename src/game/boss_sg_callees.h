// Internal to boss_sg.cpp and boss_sg_fuzz.cpp: the cells the 53 functions of
// group BSG touch that symbols.toml has no name for, and the callees nobody
// owns, by raw address. docs/boss_sg.md.
//
// Raw-address callees (the round's rebinding pass names them):
//   0x446DE0 / 0x446E00  (): the end phase's steps 1 (the win) and 2
//             (boss_h_callees.h; the harness's standard set).
// Every other callee is ours by name (the engine's, BH's BossActor_* and
// BareRet / BareRetZero / BossHook_ExitClearActor0 / BossOp_ScriptTick) or a
// .data table entry the fuzz swaps for a recorder (the generic enemy states
// 0x4365D0, 0x436620, 0x436BC0, 0x436F00, 0x437030, 0x437180, 0x437240 among
// them - reached as table entries, never called by address; the first four
// are round twelve group BE2's EnemyOp_TurnStart, _CueDispatch, _Act3Dispatch,
// _Act5Dispatch).
#pragma once

#include <cstdint>

#include "bof3/symbols.gen.h"

// Rebound 2026-09-29 (round twelve group BE4, docs/battle_e4.md section 9): the constants here naming BE4's functions read
// bof3::addr::<Name>; the values are unchanged (the fuzz keys on them).

namespace boss_sg {
namespace at {

using U = std::uint32_t;

// --- the battle's cells (0x904AA0..0x904BA0) ---------------------------------
constexpr U kFight = 0x904AAA;            // u8: the event battle (Boss_SetupTable's index)
constexpr U kFacing = 0x904AAC;           // u8: the battle's facing (inventory_ops.md; read as a dword, & 0xFF)
// kScript: now BossSe_Script (symbols.toml). u8: the fights' script bits (bit 2 set by kind 40's death flash)
// kMusicFlags: now EventBattle_Flags (symbols.toml). u8: bit 0x40 keeps the battle's music
// kBattleEnd: now Battle_EndFlags (symbols.toml). u8: bit 1 the win; bit 2 read by set-up 33; bit 3 set by set-up 32
constexpr U kActor = 0x904B34;            // u8: the acting actor (3..10 an enemy)
constexpr U kTarget = 0x904B44;           // u8: the target (0..2 a member, 3..10 an enemy)

// --- the hooks -----------------------------------------------------------------
// kHookEnd: now BattleHook_End (symbols.toml). BattleHook_End
// kHookExit: now BattleHook_Exit (symbols.toml). BattleHook_Exit
// kHookEvent: now BattleHook_Event (symbols.toml). BattleHook_Event

// --- the enemies and the task slots ---------------------------------------------
// kCurrentEnemy: now BattleE3_CurrentEnemy (symbols.toml). the enemy BattleEnemy_RunAll is running
// kEnemies: now Battle_Enemies (symbols.toml). enemy objects, stride 0x128
constexpr U kEnemyStride = 0x128;
// kTasks: now BattleTask_Slots (symbols.toml). BattleTask_Create's 48 slots of 0x84
constexpr U kTaskStride = 0x84;
// kOwner: now BattleE2_Owner (symbols.toml). the running task's owner (BattleTask_RunAll sets it from the slot's +0x80)

// --- the party (ObjTrio, stride 0x14C) -----------------------------------------
// kParty: now ObjTrio (symbols.toml).
constexpr U kPartyStride = 0x14C;
// kMember0X: now ObjTrio[0].x (symbols.toml). member 0's +0x34 (x), read by set-up 33's end hook
// kMember0Z: now ObjTrio[0].z (symbols.toml). member 0's +0x38 (z)

// --- other cells ------------------------------------------------------------
// kChapterStep: now MoveScript_PartyRecords[0x65] (symbols.toml). u8: the chapter run's step
constexpr U kPoseSet = 0x8C5D80;          // the frame set set-up 32's end hook poses the party from
// Signed byte pairs (x, z) indexed by the facing 0x904AAC (four pairs; the
// facing is read whole, & 0xFF, unchecked). Shared with engine code outside the
// band (0x44A18E, 0x44A302, 0x44A352, 0x452756 read it too - an E8 / immediate
// scan, 2026-09-28), so not named here; F6's rise reads it.
constexpr U kFacingOffsets = 0x64E4F4;

// --- the callees nobody owns ------------------------------------------------
constexpr U kEndWin = bof3::addr::BattleEnd_EnterStep1;           // () the end phase, step 1
constexpr U kEndOther = bof3::addr::BattleEnd_EnterStep2;         // () step 2

}  // namespace at
}  // namespace boss_sg
