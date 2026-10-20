// Internal to boss_sa.cpp and boss_sa_fuzz.cpp: the cells group BSA's
// functions touch that symbols.toml has no name for, the hooks and tables
// they store as literals, and the callees nobody owns, by raw address.
// docs/boss_sa.md.
//
// Raw-address callees (the round's rebinding pass names them):
//   0x437450  (unsigned id): Sound_PlayEffect(id) unless the word id is
//             0xFFFF - the enemies' cue player (read 2026-09-28,
//             0x437450..0x437461: `cmp ax, 0xFFFF`, the whole dword pushed
//             on; Sound_PlayEffect reads 16 bits); nobody owns it
//             (enemy_ai_ops_callees.h kPlayCue, battle_items_callees.h
//             kEnemySound).
//   0x4376A0  (): Sprite_Current +1 = 2, +2 = 0, Battle_ClearActorBit(+5),
//             0x939AD8's +0x110 bit 9 cleared, and without 0x904AA8 bit
//             0x40 its +0x105 = 0 - an enemy's turn closed, back to the idle
//             step (read 2026-09-28, 0x4376A0..0x4376EF); nobody owns it.
//   0x4376F0  (): with 0x939AD8's +0x90 bit 3 and 0x5B93D2's al bit 0,
//             0x904AA8 |= 0x80 and BattleTask_Create(0, 2) (read 2026-09-28,
//             0x4376F0..0x43771A); nobody owns it.
//   0x446DE0, 0x446E00, 0x446E20  (): 0x904AA0 = 5, 0x904AA2 = 0 with
//             0x904AA1 = 1 / 2 / 3 - the end phase's steps (boss_h_callees.h);
//             nobody owns them.
#pragma once

#include <cstdint>

#include "bof3/symbols.gen.h"

// Rebound 2026-09-29 (round twelve group BE4, docs/battle_e4.md section 9): the constants here naming BE4's functions read
// bof3::addr::<Name>; the values are unchanged (the fuzz keys on them).
// Rebound 2026-09-28 (round eleven's cleanup, docs/round-11-cleanup.md item 2):
// every constant here whose target has a name in symbols.toml reads
// bof3::addr::<Name>. The values are unchanged - the fuzz keys on them.

namespace boss_sa {
namespace at {

using U = std::uint32_t;

// --- the battle's cells -----------------------------------------------------
// kCurrentEnemy: now BattleE3_CurrentEnemy (symbols.toml). the enemy EnemyRunAll is running (its object)
constexpr U kRoundFlags = 0x904AA8;       // u8: the round flags' low byte
// kEventFlags: now BossSe_Script (symbols.toml). u8: bit 0 set by the hooks of kinds 1 and 39 at a quarter of HP, and by Boss01_Event
// kBattleEnd: now Battle_EndFlags (symbols.toml). u8: bit 0 the loss, bit 1 the win, bit 2 set by Gary's end (tested with 6 by set-ups 2, 3)
// kExp: now Battle_ExpTotal (symbols.toml). u32: the experience the battle pays (Battle_EnemyDefeated adds +0x96)
constexpr U kActor = 0x904B34;            // u8: the acting actor, 3.. an enemy
constexpr U kActionKind = 0x904B35;       // u8: the action's kind (4 an ability)
// kAction: now BattleSprites_Action (symbols.toml). pointer: the action's record, +2 the action's id (u16)
constexpr U kTarget = 0x904B44;           // u8: the battle's target
// kEndCount: now BossSa_EndCount (symbols.toml). u16: Mogu's end count (0x3C), Gary's end waits for it to be 0
// kChapterRun: now MoveScript_Var7 (symbols.toml). u8: the chapter's run
// kChapterStep: now MoveScript_PartyRecords[0x65] (symbols.toml). u8: the chapter run's step
// kWindowPass: now BossSa_WindowPass (symbols.toml). u8: the window records' pass byte (symbols.toml, the 0x59E230 evidence)
constexpr U kMoveVar3 = 0x903848;         // u8: MoveScript counter 0 (op A0's; the message choices' "variable 3")
// kTaskSlots: now BattleTask_Slots (symbols.toml). BattleTask_Create's slots, 0x84 bytes each
constexpr U kTaskStride = 0x84;
// kEnemies: now Battle_Enemies (symbols.toml). the enemies' objects, 0x128 bytes each
constexpr U kEnemyStride = 0x128;
// kEnemy1: now Battle_Enemies[1] (symbols.toml). enemy 1's object
constexpr U kFieldActors = 0x7DEF00;      // Sprite_Objects + 0x80: MoveCmd_OpE9's object for field object i (+ i * 0xA4)
constexpr U kObjectStride = 0xA4;

// --- the party's leader (ObjTrio + 0) ---------------------------------------
// kLeaderTarget: now ObjTrio[0x124] (symbols.toml). +0x124 the member's target
// kLeaderAction: now ObjTrio[0x125] (symbols.toml). +0x125 its action's kind
// kLeaderActionId: now ObjTrio[0x126] (symbols.toml). +0x126 (u16) its action's id
// kLeaderFlags: now ObjTrio[0x134] (symbols.toml). +0x134 (u32): bit 1

// --- the literals the functions store ---------------------------------------
// The hooks the set-ups store (BattleHook_End / _Exit / _Event), and the
// enemies' +0xF4 hooks the kinds' state 0 stores: the originals' addresses,
// which in the game are Capcom's function or the jmp Inject put there to ours.
constexpr U kBoss01End = bof3::addr::Boss01_End;
constexpr U kBoss01Exit = bof3::addr::Boss01_Exit;
constexpr U kBoss01Event = bof3::addr::Boss01_Event;
constexpr U kBoss02End = bof3::addr::Boss02_End;
constexpr U kBoss02Event = bof3::addr::Boss02_Event;
constexpr U kBoss03End = bof3::addr::Boss03_End;
constexpr U kBareRet = bof3::addr::BareRet;          // BareRet (BH's)
constexpr U kBareRetZero = bof3::addr::BareRetZero;      // BareRetZero (BH's)
constexpr U kEndPickWay = bof3::addr::BossHook_EndPickWay;       // BossHook_EndPickWay (BH's)
constexpr U kExitActor0Bit40 = bof3::addr::BossHook_ExitActor0Bit40;  // BossHook_ExitActor0Bit40 (BH's)
constexpr U kGaryHook = bof3::addr::BossGary_Hook;
constexpr U kMoguHook = bof3::addr::BossMogu_Hook;
constexpr U kNueHook = bof3::addr::BossNue_Hook;
constexpr U kNue2Hook = bof3::addr::BossNue2_Hook;
constexpr U kSample1Hook = bof3::addr::BossSample1_Hook;
constexpr U kWeretigrHook = bof3::addr::BossWeretigr_Hook;

// --- the callees nobody owns ------------------------------------------------
constexpr U kPlayCue = bof3::addr::Sound_PlayEffectUnlessNone;          // (unsigned id) Sound_PlayEffect unless 0xFFFF (BE3's since round twelve: the same value)
constexpr U kTurnClose = bof3::addr::EnemyOp_EndAction;        // () the enemy's turn closed (BE3's since round twelve: the same value)
constexpr U kTurnChance = bof3::addr::EnemyOp_RollBit80Task;       // () 0x904AA8 bit 7 and a task on a chance (BE3's since round twelve: the same value)
constexpr U kEndWin = bof3::addr::BattleEnd_EnterStep1;           // () the end phase, step 1
constexpr U kEndOther = bof3::addr::BattleEnd_EnterStep2;         // () the end phase, step 2
constexpr U kEndThird = bof3::addr::BattleEnd_EnterStep3;         // () the end phase, step 3

}  // namespace at
}  // namespace boss_sa
