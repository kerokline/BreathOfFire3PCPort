// Internal to boss_sc.cpp and boss_sc_fuzz.cpp: the cells group BSC's boss
// code touches that symbols.toml has no name for, and the callees nobody owns,
// by raw address. docs/boss_sc.md.
//
// Raw-address callees (the round's rebinding pass names them):
//   0x446DE0  (): 0x904AA0 = 5, 0x904AA1 = 1, 0x904AA2 = 0 - the end phase's
//             step 1 (the win); engine code nobody owns (boss_h_callees.h).
//   0x446E00  (): the same with step 2; nobody owns it.
//   0x446E20  (): the same with step 3; nobody owns it.
//   0x454A80  (object): releases every Field_Slots record whose +0xC is the
//             object (read by AR2B, area_w2b_callees.h, 0x454A80..0x454AAA);
//             engine code nobody owns.
//   0x455290  (object, script): the first free Field_Slots record of eight
//             (0x9035C0, stride 0x10) taken for the object with the script
//             (read 2026-09-28, 0x455290..0x4552F5; area_w2b_callees.h); al
//             0xFF when none is free - not read by our caller.
#pragma once

#include <cstdint>

#include "bof3/symbols.gen.h"   // bof3::addr: the constants below naming group BE6's functions (docs/battle_e6.md section 5)

// Rebound 2026-09-29 (round twelve group BE4, docs/battle_e4.md section 9): the constants here naming BE4's functions read
// bof3::addr::<Name>; the values are unchanged (the fuzz keys on them).

namespace boss_sc {
namespace at {

using U = std::uint32_t;

// --- the battle's cells (0x904AA0..0x904BA0, the harness's battle bytes) ---
constexpr ::bof3::image::Va kRoundFlagsHi{0x904AA9};     // u8: the round flags' high byte (0x904AA8 + 1); Amalgam's death sets bit 2
// kFight: now EventBattle_Id (symbols.toml). u8: the event battle
// kFightFlags: now BossSe_Script (symbols.toml). u8: the fight's own progress bits (fights 13 / 16: which lines were shown)
// kCount4AE2: now BossSe_RoundSlot (symbols.toml). u8: Nina's action hook decrements it once (what it counts was not read)
// kBattleEnd: now Battle_EndFlags (symbols.toml). u8: bit 1 the win
// kBattleExp: now Battle_ExpTotal (symbols.toml). u32: the battle's experience sum (Battle_EnemyDefeated adds +0x96)
constexpr ::bof3::image::Va kActor{0x904B34};            // u8: the acting actor
constexpr ::bof3::image::Va kAction{0x904B35};           // u8: the enemy's action pick
constexpr ::bof3::image::Va kTarget{0x904B44};           // u8: the battle's target

// --- a byte of the move scripts' counters (MoveScript_CounterTest's 0x903848
// + n): the end hooks store the scene the field runs next; what each value
// selects was not read.
constexpr ::bof3::image::Va kMoveCounter{0x903848};

// --- the field's cells B16's hooks write --------------------------------------
// kSaveWord: now Field_ScriptFlags (symbols.toml). u16 in the live game block (bit 7 cleared by fight 16's end hook)
// kFieldByte131: now Music_Track (symbols.toml). u8: set to 0x31 by fight 16's end hook (what reads it was not read)
// kMember1Flags: now ObjTrio (symbols.toml). record 1 +0, ObjTrio[1].flags (Field_Members[4] until stage C4). u8: ObjTrio member 1's +0 (bit 0x40 set by fight 16's exit hook)
// kLeaderPick: now BossH_LeaderPick (symbols.toml). u8: 6 / 7 (Boss_SetByLeaderId's byte; boss_h_callees.h)
// kMsgPass: now BossSa_WindowPass (symbols.toml). u8: set to 2 before each Msg_OpenScript here (the window pass byte)

// --- fight 16's counter: turns before Nina's action hook stops waiting ------
// kWaitTurns: now BossSc_WaitTurns (symbols.toml). u8: set to 8 or 10 by Boss16_Setup (Rand & 2), counted down

// --- the name Str_CopyN copies into the banner --------------------------------
// kBannerChar: now Character_IndexById[1] (symbols.toml). u8: the character record index (0x903A70 + n * 0xA4)
// kCharRecords: now CharacterRecords (symbols.toml). the live character records, stride 0xA4
constexpr U kCharStride = 0xA4;

// --- the enemies' objects (0x93B960 + n * 0x128) -----------------------------
// kEnemy0: now Battle_Enemies (symbols.toml).
// kEnemy1: now Battle_Enemies[1] (symbols.toml).
// kEnemy2: now Battle_Enemies[2] (symbols.toml).
constexpr U kEnemyPose = 0x58;            // u16 +0x58 and +0x5A: the pose a field actor is given back
constexpr U kEnemyStatus = 0x92;          // u32 from +0x92: bit 0x2000 (+0x93 bit 0x20)
constexpr U kEnemyExp = 0x96;             // u16

// --- the callees nobody owns ------------------------------------------------
constexpr U kEndWin = bof3::addr::BattleEnd_EnterStep1;           // () the end phase, step 1
constexpr U kEndOther = bof3::addr::BattleEnd_EnterStep2;         // () step 2
constexpr U kEndThird = bof3::addr::BattleEnd_EnterStep3;         // () step 3
constexpr U kSlotsReleaseFor = bof3::addr::Field_SlotsReleaseOwner;  // 0x454A80 (object; BE6, 2026-09-29)
constexpr U kSlotStart = bof3::addr::Field_SlotStart;               // 0x455290 (object, script; BE6, 2026-09-29)

}  // namespace at
}  // namespace boss_sc
