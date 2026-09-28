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

namespace boss_sc {
namespace at {

using U = std::uint32_t;

// --- the battle's cells (0x904AA0..0x904BA0, the harness's battle bytes) ---
constexpr U kRoundFlagsHi = 0x904AA9;     // u8: the round flags' high byte (0x904AA8 + 1); Amalgam's death sets bit 2
constexpr U kFight = 0x904AAA;            // u8: the event battle
constexpr U kFightFlags = 0x904AAD;       // u8: the fight's own progress bits (fights 13 / 16: which lines were shown)
constexpr U kCount4AE2 = 0x904AE2;        // u8: Nina's action hook decrements it once (what it counts was not read)
constexpr U kBattleEnd = 0x904AE8;        // u8: bit 1 the win
constexpr U kBattleExp = 0x904AEC;        // u32: the battle's experience sum (Battle_EnemyDefeated adds +0x96)
constexpr U kActor = 0x904B34;            // u8: the acting actor
constexpr U kAction = 0x904B35;           // u8: the enemy's action pick
constexpr U kTarget = 0x904B44;           // u8: the battle's target

// --- a byte of the move scripts' counters (MoveScript_CounterTest's 0x903848
// + n): the end hooks store the scene the field runs next; what each value
// selects was not read.
constexpr U kMoveCounter = 0x903848;

// --- the field's cells B16's hooks write --------------------------------------
constexpr U kSaveWord = 0x9039A2;         // u16 in the live game block (bit 7 cleared by fight 16's end hook)
constexpr U kFieldByte131 = 0x904131;     // u8: set to 0x31 by fight 16's end hook (what reads it was not read)
constexpr U kMember1Flags = 0x802E8C;     // u8: ObjTrio member 1's +0 (bit 0x40 set by fight 16's exit hook)
constexpr U kLeaderPick = 0x92BF18;       // u8: 6 / 7 (Boss_SetByLeaderId's byte; boss_h_callees.h)
constexpr U kMsgPass = 0x802D20;          // u8: set to 2 before each Msg_OpenScript here (the window pass byte)

// --- fight 16's counter: turns before Nina's action hook stops waiting ------
constexpr U kWaitTurns = 0x675F00;        // u8: set to 8 or 10 by Boss16_Setup (Rand & 2), counted down

// --- the name Str_CopyN copies into the banner --------------------------------
constexpr U kBannerChar = 0x66972D;       // u8: the character record index (0x903A70 + n * 0xA4)
constexpr U kCharRecords = 0x903A70;      // the live character records, stride 0xA4
constexpr U kCharStride = 0xA4;

// --- the enemies' objects (0x93B960 + n * 0x128) -----------------------------
constexpr U kEnemy0 = 0x93B960;
constexpr U kEnemy1 = 0x93BA88;
constexpr U kEnemy2 = 0x93BBB0;
constexpr U kEnemyPose = 0x58;            // u16 +0x58 and +0x5A: the pose a field actor is given back
constexpr U kEnemyStatus = 0x92;          // u32 from +0x92: bit 0x2000 (+0x93 bit 0x20)
constexpr U kEnemyExp = 0x96;             // u16

// --- the callees nobody owns ------------------------------------------------
constexpr U kEndWin = 0x446DE0;           // () the end phase, step 1
constexpr U kEndOther = 0x446E00;         // () step 2
constexpr U kEndThird = 0x446E20;         // () step 3
constexpr U kSlotsReleaseFor = 0x454A80;  // (object)
constexpr U kSlotStart = 0x455290;        // (object, script)

}  // namespace at
}  // namespace boss_sc
