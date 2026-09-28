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

namespace boss_sa {
namespace at {

using U = std::uint32_t;

// --- the battle's cells -----------------------------------------------------
constexpr U kCurrentEnemy = 0x939AD8;     // the enemy EnemyRunAll is running (its object)
constexpr U kRoundFlags = 0x904AA8;       // u8: the round flags' low byte
constexpr U kEventFlags = 0x904AAD;       // u8: bit 0 set by the hooks of kinds 1 and 39 at a quarter of HP, and by Boss01_Event
constexpr U kBattleEnd = 0x904AE8;        // u8: bit 0 the loss, bit 1 the win, bit 2 set by Gary's end (tested with 6 by set-ups 2, 3)
constexpr U kExp = 0x904AEC;              // u32: the experience the battle pays (Battle_EnemyDefeated adds +0x96)
constexpr U kActor = 0x904B34;            // u8: the acting actor, 3.. an enemy
constexpr U kActionKind = 0x904B35;       // u8: the action's kind (4 an ability)
constexpr U kAction = 0x904B40;           // pointer: the action's record, +2 the action's id (u16)
constexpr U kTarget = 0x904B44;           // u8: the battle's target
constexpr U kEndCount = 0x904B7E;         // u16: Mogu's end count (0x3C), Gary's end waits for it to be 0
constexpr U kChapterRun = 0x8034E4;       // u8: the chapter's run
constexpr U kChapterStep = 0x8034E5;      // u8: the chapter run's step
constexpr U kWindowPass = 0x802D20;       // u8: the window records' pass byte (symbols.toml, the 0x59E230 evidence)
constexpr U kMoveVar3 = 0x903848;         // u8: MoveScript counter 0 (op A0's; the message choices' "variable 3")
constexpr U kTaskSlots = 0x93A000;        // BattleTask_Create's slots, 0x84 bytes each
constexpr U kTaskStride = 0x84;
constexpr U kEnemies = 0x93B960;          // the enemies' objects, 0x128 bytes each
constexpr U kEnemyStride = 0x128;
constexpr U kEnemy1 = 0x93BA88;           // enemy 1's object
constexpr U kFieldActors = 0x7DEF00;      // Sprite_Objects + 0x80: MoveCmd_OpE9's object for field object i (+ i * 0xA4)
constexpr U kObjectStride = 0xA4;

// --- the party's leader (ObjTrio + 0) ---------------------------------------
constexpr U kLeaderTarget = 0x802E64;     // +0x124 the member's target
constexpr U kLeaderAction = 0x802E65;     // +0x125 its action's kind
constexpr U kLeaderActionId = 0x802E66;   // +0x126 (u16) its action's id
constexpr U kLeaderFlags = 0x802E74;      // +0x134 (u32): bit 1

// --- the literals the functions store ---------------------------------------
// The hooks the set-ups store (BattleHook_End / _Exit / _Event), and the
// enemies' +0xF4 hooks the kinds' state 0 stores: the originals' addresses,
// which in the game are Capcom's function or the jmp Inject put there to ours.
constexpr U kBoss01End = 0x437DE0;
constexpr U kBoss01Exit = 0x437E10;
constexpr U kBoss01Event = 0x437CF0;
constexpr U kBoss02End = 0x4381E0;
constexpr U kBoss02Event = 0x4381B0;
constexpr U kBoss03End = 0x438230;
constexpr U kBareRet = 0x437CC0;          // BareRet (BH's)
constexpr U kBareRetZero = 0x43C9F0;      // BareRetZero (BH's)
constexpr U kEndPickWay = 0x43EB60;       // BossHook_EndPickWay (BH's)
constexpr U kExitActor0Bit40 = 0x440820;  // BossHook_ExitActor0Bit40 (BH's)
constexpr U kGaryHook = 0x437B60;
constexpr U kMoguHook = 0x437CB0;
constexpr U kNueHook = 0x437FF0;
constexpr U kNue2Hook = 0x4380F0;
constexpr U kSample1Hook = 0x438180;
constexpr U kWeretigrHook = 0x43D630;

// --- the callees nobody owns ------------------------------------------------
constexpr U kPlayCue = 0x437450;          // (unsigned id) Sound_PlayEffect unless 0xFFFF
constexpr U kTurnClose = 0x4376A0;        // () the enemy's turn closed
constexpr U kTurnChance = 0x4376F0;       // () 0x904AA8 bit 7 and a task on a chance
constexpr U kEndWin = 0x446DE0;           // () the end phase, step 1
constexpr U kEndOther = 0x446E00;         // () the end phase, step 2
constexpr U kEndThird = 0x446E20;         // () the end phase, step 3

}  // namespace at
}  // namespace boss_sa
