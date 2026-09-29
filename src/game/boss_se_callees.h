// Internal to boss_se.cpp and boss_se_fuzz.cpp: the cells the 52 functions of
// group BSE touch that symbols.toml has no name for, and the callees nobody
// owns, by raw address. docs/boss_se.md.
//
// Raw-address callees (the round's rebinding pass names them):
//   0x437450  (unsigned sound): Sound_PlayEffect(sound) unless its low word is
//             0xFFFF - an enemy state's sound (engine code nobody owns; read
//             2026-09-28, 0x437450..0x437461).
//   0x4376A0  (): Sprite_Current +1 = 2, +2 = 0, 0x446FD0(+5), 0x939AD8's
//             +0x110 &= ~0x200, and +0x105 = 0 unless 0x904AA8 bit 6 - the end
//             of an enemy's action (engine code nobody owns; read 2026-09-28,
//             0x4376A0..0x4376EF). Kind 32's step 5 tail-jumps to it.
//   0x446DE0 / 0x446E00 / 0x446E20  (): the end phase's steps 1 (the win), 2
//             and 3 (boss_h_callees.h; the harness's standard set).
// Stored, never called (a hook a set-up installs):
//   0x43B730  set-up 23's end hook, shared with set-up 21 - group BSD's
//             (tools/boss_rows.py: 0x43B730 [B21, B23]).
#pragma once

#include <cstdint>

#include "bof3/symbols.gen.h"

// Rebound 2026-09-29 (round twelve group BE4, docs/battle_e4.md section 9): the constants here naming BE4's functions read
// bof3::addr::<Name>; the values are unchanged (the fuzz keys on them).
// Rebound 2026-09-28 (round eleven's cleanup, docs/round-11-cleanup.md item 2):
// every constant here whose target has a name in symbols.toml reads
// bof3::addr::<Name>. The values are unchanged - the fuzz keys on them.

namespace boss_se {
namespace at {

using U = std::uint32_t;

// --- the battle's cells (0x904AA0..0x904BA0) ---------------------------------
constexpr U kPhase = 0x904AA0;            // u8: Battle_PhaseDispatch's phase
constexpr U kStep = 0x904AA1;             // u8: the phase's step
constexpr U kFlags = 0x904AA8;            // u16: the round flags (bit 1, 2, 6, 7 read or set here)
constexpr U kFight = 0x904AAA;            // u8: the event battle (Boss_SetupTable's index)
constexpr U kScript = 0x904AAD;           // u8: the fights' script bits (read and set by the hooks below)
constexpr U kOrder = 0x904ACC;            // u8[]: the turn order (battle_damage.md)
constexpr U kRoundSlot = 0x904AE2;        // u8: the turn order's cursor (battle_actions.md) - the round's count here
constexpr U kOrderCount = 0x904AE3;       // u8: the turn order's length
constexpr U kInitiative = 0x904AE4;       // u8: 1 / 2 the side that moves first (battle_phases.md), 3 here
constexpr U kMusicFlags = 0x904AE5;       // u8: bit 0x40 keeps the battle's music
constexpr U kBattleEnd = 0x904AE8;        // u8: bit 1 the win; bits 2 and 3 set by hooks here
constexpr U kActor = 0x904B34;            // u8: the acting actor
constexpr U kActKind = 0x904B35;          // u8: the acting kind (battle_actions.md)
constexpr U kTarget = 0x904B44;           // u8: the target (0..2 a member, 3..10 an enemy)
constexpr U kTurn = 0x904B90;             // u32: the turn counter (battle_phases.md)

// --- the hooks and the stored literals ----------------------------------------
constexpr U kHookEnd = 0x904B64;          // BattleHook_End
constexpr U kHookExit = 0x904B68;         // BattleHook_Exit
constexpr U kHookEvent = 0x904B6C;        // BattleHook_Event
constexpr U kEnd21 = bof3::addr::Boss21_End;            // set-up 21 / 23's end hook (BSD's)

// --- the enemies' objects (0x93B960 + n * 0x128), the offsets used ------------
constexpr U kCurrentEnemy = 0x939AD8;     // the enemy BattleEnemy_RunAll is running
constexpr U kEnemy0 = 0x93B960;           // enemy 0's object
constexpr U kEnemy0State1 = 0x93B961;     // its +1
constexpr U kEnemy0State2 = 0x93B962;     // its +2
constexpr U kEnemy0Flag8E = 0x93B9EE;     // its +0x8E
constexpr U kEnemy0Hp = 0x93BA04;         // its +0xA4 (HP)
constexpr U kEnemy0Byte104 = 0x93BA64;    // its +0x104
constexpr U kEnemy0Byte105 = 0x93BA65;    // its +0x105
constexpr U kEnemy0Word108 = 0x93BA68;    // its +0x108 (s16)
constexpr U kEnemy0Flags = 0x93BA70;      // its +0x110 (u32)

// The three words kinds 30 and 31's entry copies into the enemy's HP and stat
// words (+0xA4 / +0xD0 / +0xB0, +0xD4 / +0xB4, +0xD6 / +0xB6); their writer
// was not read.
constexpr U kCarryHp = 0x903F0C;          // u16
constexpr U kCarryB4 = 0x903F10;          // u16
constexpr U kCarryB6 = 0x903F12;          // u16

// --- the party (ObjTrio, stride 0x14C) -----------------------------------------
constexpr U kParty = 0x802D40;
constexpr U kPartyStride = 0x14C;
constexpr U kMsgMode = 0x802D20;          // u8: set to 2 before each Msg_OpenScript here (its reader not read)
constexpr U kMember0State1 = 0x802D41;    // member 0's +1
constexpr U kMember0State2 = 0x802D42;    // member 0's +2
constexpr U kMember0Hp = 0x802DD8;        // member 0's +0x98 (u16 HP, battle_phases.md)
constexpr U kMember0Ap = 0x802DDA;        // member 0's +0x9A (u16 AP)
constexpr U kMember0Bc = 0x802DFC;        // member 0's +0xBC
constexpr U kMember0Ba = 0x802DFA;        // member 0's +0xBA
constexpr U kMember0MaxHp = 0x802DE0;     // member 0's +0xA0 (u16 max HP)
constexpr U kMember0MaxAp = 0x802DE2;     // member 0's +0xA2 (u16)
constexpr U kMember0Target = 0x802E64;    // member 0's +0x124
constexpr U kMember0Action = 0x802E65;    // member 0's +0x125 (battle_windows.md: the action)
constexpr U kMember0Skill = 0x802E66;     // member 0's +0x126 (u16: the skill)
constexpr U kMember0Word128 = 0x802E68;   // member 0's +0x128 (s16; +0x128 of the member a target names)
constexpr U kMember0Word12A = 0x802E6A;   // member 0's +0x12A

// --- set-up 25's save of member 0's HP and AP (one byte each) -----------------
constexpr U kSavedAp = 0x675F04;          // u8 (.data)
constexpr U kSavedHp = 0x675F05;          // u8 (.data)

// --- set-up 25's exit copy and set-up 26's counters (0x939A00..) --------------
constexpr U kPartyCopyCount = 0x939A02;   // u8: bytes to copy
constexpr U kPartyCopyFrom = 0x939A10;    // the bytes
constexpr U kPartyList = 0x904065;        // the party list (area_harness.md: Cond_Flags' party lists)
constexpr U kSumEnemy = 0x939A04;         // u32: set-up 26's sum of enemy 0's s16 +0x108 (targeted, not flagged)
constexpr U kSumMember = 0x939A08;        // u32: set-up 26's sum of a targeted member's s16 +0x128 (enemy 0 acting)
constexpr U kCountFlagged = 0x939A0C;     // u8: set-up 26's count of enemy 0 targeted with its +0x110 bit 1

// --- other cells ------------------------------------------------------------
constexpr U kScriptVar3 = 0x903848;       // u8: movement-script variable 3 (area_w0a.md), set by the end hooks
constexpr U kLeaderPick = 0x92BF18;       // u8: the byte Boss_SetByLeaderId writes; 3 here
constexpr U kChapterStep = 0x8034E5;      // u8: the chapter run's step
constexpr U kPoseSet = 0x8C5D80;          // the frame set set-up 26's end hook poses the party from

// --- the callees nobody owns ------------------------------------------------
constexpr U kEnemySound = 0x437450;       // (sound)
constexpr U kEnemyActEnd = 0x4376A0;      // ()
constexpr U kEndWin = bof3::addr::BattleEnd_EnterStep1;           // () the end phase, step 1
constexpr U kEndOther = bof3::addr::BattleEnd_EnterStep2;         // () step 2
constexpr U kEndThird = bof3::addr::BattleEnd_EnterStep3;         // () step 3

}  // namespace at
}  // namespace boss_se
