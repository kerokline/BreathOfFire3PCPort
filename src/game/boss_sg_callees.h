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

namespace boss_sg {
namespace at {

using U = std::uint32_t;

// --- the battle's cells (0x904AA0..0x904BA0) ---------------------------------
constexpr U kFight = 0x904AAA;            // u8: the event battle (Boss_SetupTable's index)
constexpr U kFacing = 0x904AAC;           // u8: the battle's facing (inventory_ops.md; read as a dword, & 0xFF)
constexpr U kScript = 0x904AAD;           // u8: the fights' script bits (bit 2 set by kind 40's death flash)
constexpr U kMusicFlags = 0x904AE5;       // u8: bit 0x40 keeps the battle's music
constexpr U kBattleEnd = 0x904AE8;        // u8: bit 1 the win; bit 2 read by set-up 33; bit 3 set by set-up 32
constexpr U kActor = 0x904B34;            // u8: the acting actor (3..10 an enemy)
constexpr U kTarget = 0x904B44;           // u8: the target (0..2 a member, 3..10 an enemy)

// --- the hooks -----------------------------------------------------------------
constexpr U kHookEnd = 0x904B64;          // BattleHook_End
constexpr U kHookExit = 0x904B68;         // BattleHook_Exit
constexpr U kHookEvent = 0x904B6C;        // BattleHook_Event

// --- the enemies and the task slots ---------------------------------------------
constexpr U kCurrentEnemy = 0x939AD8;     // the enemy BattleEnemy_RunAll is running
constexpr U kEnemies = 0x93B960;          // enemy objects, stride 0x128
constexpr U kEnemyStride = 0x128;
constexpr U kTasks = 0x93A000;            // BattleTask_Create's 48 slots of 0x84
constexpr U kTaskStride = 0x84;
constexpr U kOwner = 0x93B940;            // the running task's owner (BattleTask_RunAll sets it from the slot's +0x80)

// --- the party (ObjTrio, stride 0x14C) -----------------------------------------
constexpr U kParty = 0x802D40;
constexpr U kPartyStride = 0x14C;
constexpr U kMember0X = 0x802D74;         // member 0's +0x34 (x), read by set-up 33's end hook
constexpr U kMember0Z = 0x802D78;         // member 0's +0x38 (z)

// --- other cells ------------------------------------------------------------
constexpr U kChapterStep = 0x8034E5;      // u8: the chapter run's step
constexpr U kPoseSet = 0x8C5D80;          // the frame set set-up 32's end hook poses the party from
// Signed byte pairs (x, z) indexed by the facing 0x904AAC (four pairs; the
// facing is read whole, & 0xFF, unchecked). Shared with engine code outside the
// band (0x44A18E, 0x44A302, 0x44A352, 0x452756 read it too - an E8 / immediate
// scan, 2026-09-28), so not named here; F6's rise reads it.
constexpr U kFacingOffsets = 0x64E4F4;

// --- the callees nobody owns ------------------------------------------------
constexpr U kEndWin = 0x446DE0;           // () the end phase, step 1
constexpr U kEndOther = 0x446E00;         // () step 2

}  // namespace at
}  // namespace boss_sg
