// Internal to boss_sh.cpp and boss_sh_fuzz.cpp: the cells the 46 functions of
// group BSH touch that symbols.toml has no name for, and the callees nobody
// owns, by raw address. docs/boss_sh.md.
//
// Raw-address callees (the round's rebinding pass names them):
//   0x446700  (unsigned char actor): 0x904AE2 -= 1, then the turn order
//             0x904ACC[0x904AE2] = the actor - the actor put at the front of
//             what is left of the round's order (engine code nobody owns; read
//             2026-09-28, 0x446700..0x44671A). Set-up 34's event hook, code 5.
//   0x437450  (unsigned sound): Sound_PlayEffect(sound) unless its low word is
//             0xFFFF - an enemy state's sound (read 2026-09-28,
//             0x437450..0x437461; boss_se_callees.h has the same).
//   0x4376A0  (): Sprite_Current +1 = 2, +2 = 0, Battle_ClearActorBit(+5),
//             0x939AD8's +0x110 &= ~0x200, +0x105 = 0 unless 0x904AA8 bit 6 -
//             the end of an enemy's action (read 2026-09-28, 0x4376A0..0x4376EF).
//   0x4376F0  (): with 0x939AD8's +0x90 bit 3 and Rand's bit 0, 0x904AA8 |=
//             0x80 and BattleTask_Create(0, 2) (read 2026-09-28,
//             0x4376F0..0x43771A).
//   0x446DE0 / 0x446E00  (): the end phase's steps 1 (the win) and 2 (the
//             harness's standard set).
// Stored, never called (a hook a set-up installs):
//   0x43E790  set-up 35's exit hook, shared with set-up 26 - group BSE's
//             BossHook_ExitTransition4 (ours; stored as the literal).
//   0x43EB60  BH's BossHook_EndPickWay (set-ups 41, 43, 47's end hook).
//   0x437CC0  BH's BareRet, 0x43C9F0 BH's BareRetZero.
#pragma once

#include <cstdint>

#include "bof3/symbols.gen.h"
// Rebound 2026-09-28 (round eleven's cleanup, docs/round-11-cleanup.md item 2):
// every constant here whose target has a name in symbols.toml reads
// bof3::addr::<Name>. The values are unchanged - the fuzz keys on them.

namespace boss_sh {
namespace at {

using U = std::uint32_t;

// --- the battle's cells (0x904AA0..0x904BA0) ---------------------------------
constexpr U kFlags = 0x904AA8;            // u16: the round flags (bit 1 cleared by Gaist's hook, bit 2 the action's done flag, bit 15 read)
constexpr U kScript = 0x904AAD;           // u8: the fights' script bits (set-up 34: bit 0 the member put first, bit 1 the ability given)
constexpr U kRoundSlot = 0x904AE2;        // u8: the turn order's cursor (0x446700 decrements it)
constexpr U kBattleEnd = 0x904AE8;        // u8: bit 1 the win; bit 3 set by set-up 34's end hook
constexpr U kActor = 0x904B34;            // u8: the acting actor (0..2 a member, 3..10 an enemy)
constexpr U kActKind = 0x904B35;          // u8: the acting kind (battle_actions.md)
constexpr U kAction = 0x904B40;           // unsigned char *: the action record set-up 34's hook writes +1 and +2 of
constexpr U kTarget = 0x904B44;           // u8: the target
constexpr U kWord80 = 0x904B80;           // u16: set to 0x40 beside the action record's +2 (its reader not read)

// --- the hooks -------------------------------------------------------------------
constexpr U kHookEnd = 0x904B64;          // BattleHook_End
constexpr U kHookExit = 0x904B68;         // BattleHook_Exit
constexpr U kHookEvent = 0x904B6C;        // BattleHook_Event
constexpr U kExitTransition4 = bof3::addr::BossHook_ExitTransition4;  // BossHook_ExitTransition4 (BSE's): set-up 35's exit hook

// --- set-up 34's member pick ------------------------------------------------------
constexpr U kPickId = 0x669730;           // u8: the byte a member's +0x148 is compared with (written by scenario code)
constexpr U kPicked = 0x675F08;           // u8 (read once as a dword): the member whose +0x148 matched; only set-up 34 writes it
constexpr U kPartyTag = 0x802E88;         // member 0's +0x148 (ObjTrio stride 0x14C; symbols.toml's Field_Members)
constexpr U kParty = 0x802D40;            // ObjTrio
constexpr U kPartyStride = 0x14C;
constexpr U kPartyFlags91 = 0x802DD1;     // member 0's +0x91 (bit 0x20 read by set-up 34's event hook)

// --- the enemies (0x93B960 + n * 0x128) ---------------------------------------------
constexpr U kCurrentEnemy = 0x939AD8;     // the enemy BattleEnemy_RunAll is running
constexpr U kEnemies = 0x93B960;
constexpr U kEnemyStride = 0x128;
constexpr U kEnemy0X = 0x93B9B8;          // enemy 0's word +0x58 (set-up 34's exit copies it)
constexpr U kEnemy0Y = 0x93B9BA;          // enemy 0's word +0x5A
constexpr U kEnemyDataCount9 = 0x8C5652;  // the area's enemy data records' byte +0x8A (stride 0x8C): the count step 4 starts with

// --- the effect task slots (BattleTask_Create's) -----------------------------------
constexpr U kTasks = 0x93A000;
constexpr U kTaskStride = 0x84;
constexpr unsigned kTaskCount = 48;

// --- the chapter -------------------------------------------------------------------
constexpr U kChapterStep = 0x8034E5;      // u8: the chapter run's step
constexpr U kPoseSet = 0x8C5D80;          // the frame set set-up 34's end hook poses the party from

// --- the kinds' byte tables stored as literals (+0xFC / +0xF8) -----------------------
constexpr U kMikbaAnims = 0x64D6CC;       // kind 40's +0xFC bytes (group BSG's kind), kind 48 stores it too
constexpr U kMikbaSounds = 0x64D6D8;      // kind 40's +0xF8 words, kind 48 stores it too
constexpr U kTorchAnims = 0x675F0C;       // kind 42's +0xFC: 12 bytes of .data nothing in the exe writes (zeros)
constexpr U kTorchSounds = 0x64D7A0;      // kind 42's +0xF8: 0xFFFF words (no sound)

// --- the callees nobody owns ----------------------------------------------------------
constexpr U kOrderFront = 0x446700;       // (actor)
constexpr U kEnemySound = bof3::addr::Sound_PlayEffectUnlessNone;       // (sound) (BE3's since round twelve: the same value)
constexpr U kEnemyActEnd = bof3::addr::EnemyOp_EndAction;      // () (BE3's since round twelve: the same value)
constexpr U kEnemyActChance = bof3::addr::EnemyOp_RollBit80Task;   // () (BE3's since round twelve: the same value)
constexpr U kEndWin = 0x446DE0;           // () the end phase, step 1
constexpr U kEndOther = 0x446E00;         // () step 2

}  // namespace at
}  // namespace boss_sh
