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
constexpr ::bof3::image::Va kPhase{0x904AA0};            // u8: Battle_PhaseDispatch's phase
// kStep: now BattleActions_Step (symbols.toml). u8: the phase's step
constexpr ::bof3::image::Va kFlags{0x904AA8};            // u16: the round flags (bit 1, 2, 6, 7 read or set here)
// kFight: now EventBattle_Id (symbols.toml). u8: the event battle (Boss_SetupTable's index)
// kScript: now BossSe_Script (symbols.toml). u8: the fights' script bits (read and set by the hooks below)
constexpr ::bof3::image::Va kOrder{0x904ACC};            // u8[]: the turn order (battle_damage.md)
// kRoundSlot: now BossSe_RoundSlot (symbols.toml). u8: the turn order's cursor (battle_actions.md) - the round's count here
// kOrderCount: now BattleDamage_TurnCount (symbols.toml). u8: the turn order's length
// kInitiative: now BattlePhases_Initiative (symbols.toml). u8: 1 / 2 the side that moves first (battle_phases.md), 3 here
// kMusicFlags: now EventBattle_Flags (symbols.toml). u8: bit 0x40 keeps the battle's music
// kBattleEnd: now Battle_EndFlags (symbols.toml). u8: bit 1 the win; bits 2 and 3 set by hooks here
constexpr ::bof3::image::Va kActor{0x904B34};            // u8: the acting actor
constexpr ::bof3::image::Va kActKind{0x904B35};          // u8: the acting kind (battle_actions.md)
constexpr ::bof3::image::Va kTarget{0x904B44};           // u8: the target (0..2 a member, 3..10 an enemy)
constexpr ::bof3::image::Va kTurn{0x904B90};             // u32: the turn counter (battle_phases.md)

// --- the hooks and the stored literals ----------------------------------------
// kHookEnd: now BattleHook_End (symbols.toml). BattleHook_End
// kHookExit: now BattleHook_Exit (symbols.toml). BattleHook_Exit
// kHookEvent: now BattleHook_Event (symbols.toml). BattleHook_Event
constexpr ::bof3::image::CodeVa kEnd21{bof3::addr::Boss21_End};            // set-up 21 / 23's end hook (BSD's)

// --- the enemies' objects (0x93B960 + n * 0x128), the offsets used ------------
// kCurrentEnemy: now BattleE3_CurrentEnemy (symbols.toml). the enemy BattleEnemy_RunAll is running
// kEnemy0: now Battle_Enemies (symbols.toml). enemy 0's object
constexpr ::bof3::image::Va kEnemy0State1{0x93B961};     // its +1
constexpr ::bof3::image::Va kEnemy0State2{0x93B962};     // its +2
constexpr ::bof3::image::Va kEnemy0Flag8E{0x93B9EE};     // its +0x8E
constexpr ::bof3::image::Va kEnemy0Hp{0x93BA04};         // its +0xA4 (HP)
constexpr ::bof3::image::Va kEnemy0Byte104{0x93BA64};    // its +0x104
constexpr ::bof3::image::Va kEnemy0Byte105{0x93BA65};    // its +0x105
constexpr ::bof3::image::Va kEnemy0Word108{0x93BA68};    // its +0x108 (s16)
constexpr ::bof3::image::Va kEnemy0Flags{0x93BA70};      // its +0x110 (u32)

// The three words kinds 30 and 31's entry copies into the enemy's HP and stat
// words (+0xA4 / +0xD0 / +0xB0, +0xD4 / +0xB4, +0xD6 / +0xB6); their writer
// was not read.
// kCarryHp: now BossSe_CarryHp (symbols.toml). u16
// kCarryB4: now BossSe_CarryB4 (symbols.toml). u16
// kCarryB6: now BossSe_CarryB6 (symbols.toml). u16

// --- the party (ObjTrio, stride 0x14C) -----------------------------------------
// kParty: now ObjTrio (symbols.toml).
constexpr U kPartyStride = 0x14C;
// kMsgMode: now BossSa_WindowPass (symbols.toml). u8: set to 2 before each Msg_OpenScript here (its reader not read)
// kMember0State1: now ObjTrio[0].state (symbols.toml). member 0's +1
// kMember0State2: now ObjTrio[0].step (symbols.toml). member 0's +2
// kMember0Hp: now ObjTrio[0].hp (symbols.toml). member 0's +0x98 (u16 HP, battle_phases.md)
// kMember0Ap: now ObjTrio[0].ap (symbols.toml). member 0's +0x9A (u16 AP)
// kMember0Bc: now ObjTrio_addr + 0xbc (symbols.toml). member 0's +0xBC
// kMember0Ba: now ObjTrio_addr + 0xba (symbols.toml). member 0's +0xBA
// kMember0MaxHp: now ObjTrio[0].max_hp (symbols.toml). member 0's +0xA0 (u16 max HP)
// kMember0MaxAp: now ObjTrio[0].max_ap (symbols.toml). member 0's +0xA2 (u16)
// kMember0Target: now ObjTrio_addr + 0x124 (symbols.toml). member 0's +0x124
// kMember0Action: now ObjTrio_addr + 0x125 (symbols.toml). member 0's +0x125 (battle_windows.md: the action)
// kMember0Skill: now ObjTrio_addr + 0x126 (symbols.toml). member 0's +0x126 (u16: the skill)
// kMember0Word128: now ObjTrio_addr + 0x128 (symbols.toml). member 0's +0x128 (s16; +0x128 of the member a target names)
// kMember0Word12A: now ObjTrio_addr + 0x12a (symbols.toml). member 0's +0x12A

// --- set-up 25's save of member 0's HP and AP (one byte each) -----------------
// kSavedAp: now BossSe_SavedAp (symbols.toml). u8 (.data)
// kSavedHp: now BossSe_SavedHp (symbols.toml). u8 (.data)

// --- set-up 25's exit copy and set-up 26's counters (0x939A00..) --------------
// kPartyCopyCount: now BossSe_PartyCopyCount (symbols.toml). u8: bytes to copy
constexpr ::bof3::image::Va kPartyCopyFrom{0x939A10};    // the bytes
// kPartyList: now Cond_Flags[0xd5] (symbols.toml). the party list (area_harness.md: Cond_Flags' party lists)
// kSumEnemy: now BattleE1_SumA (symbols.toml). u32: set-up 26's sum of enemy 0's s16 +0x108 (targeted, not flagged)
// kSumMember: now BattleE1_SumB (symbols.toml). u32: set-up 26's sum of a targeted member's s16 +0x128 (enemy 0 acting)
// kCountFlagged: now BattleE1_Count (symbols.toml). u8: set-up 26's count of enemy 0 targeted with its +0x110 bit 1

// --- other cells ------------------------------------------------------------
constexpr ::bof3::image::Va kScriptVar3{0x903848};       // u8: movement-script variable 3 (area_w0a.md), set by the end hooks
// kLeaderPick: now BossH_LeaderPick (symbols.toml). u8: the byte Boss_SetByLeaderId writes; 3 here
// kChapterStep: now MoveScript_PartyRecords[0x65] (symbols.toml). u8: the chapter run's step
constexpr ::bof3::image::Va kPoseSet{0x8C5D80};          // the frame set set-up 26's end hook poses the party from

// --- the callees nobody owns ------------------------------------------------
constexpr ::bof3::image::CodeVa kEnemySound{bof3::addr::Sound_PlayEffectUnlessNone};       // (sound) (BE3's since round twelve: the same value)
constexpr ::bof3::image::CodeVa kEnemyActEnd{bof3::addr::EnemyOp_EndAction};      // () (BE3's since round twelve: the same value)
constexpr ::bof3::image::CodeVa kEndWin{bof3::addr::BattleEnd_EnterStep1};           // () the end phase, step 1
constexpr ::bof3::image::CodeVa kEndOther{bof3::addr::BattleEnd_EnterStep2};         // () step 2
constexpr ::bof3::image::CodeVa kEndThird{bof3::addr::BattleEnd_EnterStep3};         // () step 3

}  // namespace at
}  // namespace boss_se
