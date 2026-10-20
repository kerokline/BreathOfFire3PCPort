// Internal to boss_si.cpp and boss_si_fuzz.cpp: the cells group BSI's boss
// functions touch that the harness or symbols.toml has no name for, and the
// callees nobody owns, by raw address. docs/boss_si.md.
//
// Raw-address callees (the round's rebinding pass names them):
//   0x446DE0  (): 0x904AA0 = 5, 0x904AA1 = 1, 0x904AA2 = 0 - the end phase's
//             step 1; engine code nobody owns (docs/boss_h.md section 9).
//   0x446E00  (): the same with step 2; nobody owns it.
//   0x4376A0  (): the turn closed (+1 = 2, +2 = 0 of Sprite_Current); engine
//             code nobody owns (docs/boss_sa.md section 8).
//   0x4376F0  (): a chance of 0x904AA8 bit 7 and a task; nobody owns it.
#pragma once

#include <cstdint>

#include "bof3/symbols.gen.h"

// Rebound 2026-09-29 (round twelve group BE4, docs/battle_e4.md section 9): the constants here naming BE4's functions read
// bof3::addr::<Name>; the values are unchanged (the fuzz keys on them).
// Rebound 2026-09-28 (round eleven's cleanup, docs/round-11-cleanup.md item 2):
// every constant here whose target has a name in symbols.toml reads
// bof3::addr::<Name>. The values are unchanged - the fuzz keys on them.

namespace boss_si {
namespace at {

using U = std::uint32_t;

// --- the battle's cells (0x904AA0..0x904BA0) --------------------------------
constexpr U kFlags = 0x904AA8;            // u8: the round flags' low byte (bit 2 read and set here)
constexpr U kFight = 0x904AAA;            // u8: the event battle
// kBattleEnd: now Battle_EndFlags (symbols.toml). u8: bit 0 the loss, bit 1 the win
constexpr U kTarget = 0x904B44;           // u8: the target (set-up 38 writes 0x40, the enemies' side)
// kHookEnd: now BattleHook_End (symbols.toml). BattleHook_End
// kHookExit: now BattleHook_Exit (symbols.toml). BattleHook_Exit
// kHookEvent: now BattleHook_Event (symbols.toml). BattleHook_Event
// kCurrentEnemy: now BattleE3_CurrentEnemy (symbols.toml). the enemy BattleEnemy_RunAll is running (its object)
// kOwner: now BattleE2_Owner (symbols.toml). the running task slot's owner (BattleTask_RunAll sets it from the slot's +0x80)

// --- the enemies' objects (0x93B960 + n * 0x128) and the task slots ----------
constexpr U kEnemy7 = 0x93C178;           // enemy 7's object: set-up 38 makes it Sprite_Current
constexpr U kEnemy0Type = 0x93BA50;       // enemy 0's +0xF0 (its enemy data record's index): the Arwan task reads it
constexpr U kTaskOwners = 0x93A080;       // the task slots' +0x80 (their owner), stride 0x84
constexpr U kTaskStride = 0x84;
constexpr unsigned kTaskCount = 48;
constexpr U kEnemyDataCount = 0x8C5652;   // the area's enemy data records' +0x8A byte (0x8C55C8 + 0x8A), stride 0x8C
constexpr U kEnemyDataStride = 0x8C;

// --- cells outside the battle bytes ------------------------------------------
constexpr U kScriptVar3 = 0x903848;       // the movement script's variable 3 (field-modes.md)
// kCentreX: now BossSi_CentreX (symbols.toml). i32: the fight's centre x (field_hidden_callees.h)
// kCentreZ: now BossSi_CentreZ (symbols.toml). i32: its z
// kKind2Z: now Field_Kind2Z (symbols.toml). i32 Field_Kind2Z
// kKind2X: now Field_Kind2X (symbols.toml). i32 Field_Kind2X
// kF3Divisor: now MoveScript_F3Divisor (symbols.toml). i16 MoveScript_F3Divisor
// kMusicTrack: now Music_Track (symbols.toml). u8 Music_Track
// kChapterStep: now MoveScript_PartyRecords[0x65] (symbols.toml). u8: the chapter's step

// --- the callees nobody owns -------------------------------------------------
constexpr U kEndWin = bof3::addr::BattleEnd_EnterStep1;           // () the end phase, step 1
constexpr U kEndOther = bof3::addr::BattleEnd_EnterStep2;         // () the end phase, step 2
constexpr U kTurnClose = bof3::addr::EnemyOp_EndAction;        // () the turn closed (BE3's since round twelve: the same value)
constexpr U kTurnChance = bof3::addr::EnemyOp_RollBit80Task;       // () a chance of 0x904AA8 bit 7 and a task (BE3's since round twelve: the same value)

// --- the Arwan task's stack table (BossArwanFx_Dispatch) ---------------------
constexpr U kFxStart = bof3::addr::BossArwanFx_Start;          // BossArwanFx_Start
constexpr U kFxCount = bof3::addr::BossArwanFx_Count;          // BossArwanFx_Count
constexpr U kFxFinish = bof3::addr::BossArwanFx_Finish;         // BossArwanFx_Finish
constexpr U kFxDone = bof3::addr::MagicFx_DoneAndFree;           // MagicFx_DoneAndFree (round nine's, ours)

}  // namespace at
}  // namespace boss_si
