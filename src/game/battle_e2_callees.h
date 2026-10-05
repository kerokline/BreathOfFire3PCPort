// Internal to battle_e2.cpp and battle_e2_fuzz.cpp: the cells group BE2's
// battle-engine code touches that symbols.toml has no name for, and the
// callees another group of round twelve's wave one owns, by raw address (the
// coordinator's rebinding pass after both merge names them). docs/battle_e2.md.
//
// Raw-address callees (cross-group edges, docs/boss_harness.md section 10.6):
//   0x442310  (): BE3's - recomputes Field_State's member (Char_RecalcStats on
//             its +0x148 record, then the party's rows); called by the two
//             restore steps after a member's record is put back.
//   0x453300  (actor): BE6's - reads the argument's low byte (a member 0..2);
//             called by the two restore steps with Sprite_Current +5.
//   0x446770  (sprite): BE4's - turns the sprite's velocity pair +0xC / +0x10
//             by its facing +8 (1: (-y, x), 2: (-x, -y), 3: (y, -x), other:
//             unchanged); the enemy slide and knock-back read the pair after.
//   0x437450  (cue): BE3's - Sound_PlayEffect(cue) unless its low word is
//             0xFFFF.
//   0x4376F0  (): BE3's - with the current enemy's +0x90 bit 3 and an odd
//             Rand, 0x904AA8 |= 0x80 and BattleTask_Create(0, 2).
//   0x4376A0  (): BE3's - the enemy op's end: +1 = 2, +2 = 0,
//             Battle_ClearActorBit(+5), the enemy's +0x110 bit 9 cleared, its
//             +0x105 = 0 unless 0x904AA8 bit 6.
//   0x452DD0  (actor): Battle_AutoTargetCheck, R4A's, ours (a standard recorder by
//             address) - reads the argument's low byte; al 1 unless the round
//             flags' bit 14 and the actor tests say otherwise.
#pragma once

#include <cstdint>

#include "bof3/symbols.gen.h"   // the constants below name their functions since 2026-10-01 (round twelve's debt 2): the same values, so the fuzz keys stand

namespace battle_e2 {
namespace at {

using U = std::uint32_t;

// --- raw callees (above) ------------------------------------------------------
constexpr U kMemberRecalc = bof3::addr::BattleParty_RecalcStats;     // BE3
constexpr U kMemberStatusSet = bof3::addr::Battle_RecalcStats;  // BE6
constexpr U kTurnVelocity = bof3::addr::Battle_TurnVectorC;     // BE4
constexpr U kPlayCue = bof3::addr::Sound_PlayEffectUnlessNone;          // BE3
constexpr U kEnemyTaskChance = bof3::addr::EnemyOp_RollBit80Task;  // BE3
constexpr U kEnemyOpEnd = bof3::addr::EnemyOp_EndAction;       // BE3
constexpr U kActorMayAct = bof3::addr::Battle_AutoTargetCheck;      // R4A's (round 14), ours

// --- the battle bytes (the harness's 0x904AA0..0x904BA0) ----------------------
constexpr U kRoundFlags = 0x904AA8;       // u16 / u32: the round flags (0x400, 0x4000, 0x8000 read here)
constexpr U kFight = 0x904AAA;            // u8: the event battle, 0 none
constexpr U kTapCommand = 0x904AA6;       // u8: 0xFF none (battle_phases_callees.h kTapCommand)
constexpr U kPartyCount = 0x904AB0;       // u8 (read as a dword, the low byte): members in the fight
constexpr U kPartyIn = 0x904AB1;          // u8: members counted in
constexpr U kEnemiesLeft = 0x904AB3;      // u8
constexpr U kActor = 0x904B34;            // u8: the acting actor
constexpr U kActKind = 0x904B35;          // u8: the action's kind (1 attack, 2, 3, 4 an ability)
constexpr U kTarget = 0x904B44;           // u8: the target, 0xFF none
constexpr U kMagicId = 0x904B80;          // u16: the ability or spell id
constexpr U kPickList = 0x904B84;         // u8[3]: the list effect 9 compares its +0xB with
constexpr U kPickCount = 0x904B87;        // u8: entries of that list
constexpr U kSavedCount = 0x904B8F;       // u8: the party count Accession kept (magic_s33.cpp)
constexpr U kPlace94 = 0x904B94;          // u8, u8: copied into 0x9045FD / 0x9045FE
constexpr U kPlace95 = 0x904B95;

// --- the party's formation (inventory_ops_callees.h kFormation) --------------
constexpr U kFormation = 0x904060;        // u8 (read as a dword, the low byte)

// --- the party (ObjTrio, stride 0x14C) and its backup (0x939AE0, the same
// stride: battle_turn_steps_callees.h kCopies) ------------------------------------
constexpr U kParty = 0x802D40, kPartyStride = 0x14C;
constexpr U kBackup = 0x939AE0;
constexpr U kBackupFlags = 0x939C14;      // kBackup + 0x134
constexpr U kBackupBit40 = 0x939B71;      // kBackup + 0x91 (bit 0x40 tested)

// --- the enemies' objects (0x93B960, stride 0x128) -----------------------------
constexpr U kEnemies = 0x93B960, kEnemyStride = 0x128;

// --- the task slots (0x93A000, stride 0x84; +0x80 the owner) --------------------
constexpr U kTasks = 0x93A000, kTaskStride = 0x84;
constexpr U kOwner = 0x93B940;            // the running slot's owner (a pointer)
constexpr U kEnemyCurrent = 0x939AD8;     // the current enemy (a pointer)

// --- the window records (WindowRecords 0x803160, stride 0x24) ------------------
constexpr U kWindow1Count = 0x80318E;     // record 1 (0x803184) +0xA: the party count
constexpr U kWindow1X = 0x803188;         // record 1 +4 (u16): x by the count
constexpr U kPartyGauges = 0x80333F;      // record 13 + i (0x803334 + 0x24 i) +0xB / +0xC: HP / AP gauge bytes
constexpr U kPartyValues = 0x803348;      // the same records +0x14 / +0x16: u16 HP, u16 AP
constexpr U kWindow18 = 0x8033E8;         // record 18: +0 in use, +4 / +6 x, y, +0x12 a row base (s16), +0x1E its copy
constexpr U kWindow19 = 0x80340C;         // record 19: the same fields
constexpr U kWindow21 = 0x803454;         // record 21: +0 in use, +4 / +6 x, y

// --- the pointer the sprite pose pool lives at while a task draws --------------
constexpr U kAnimSet = 0x9039D8;
constexpr U kAnimSetBattle = 0x8C5D80;
constexpr U kAnimSetField = 0x8B3580;

// --- tables read in place (.data) ----------------------------------------------
constexpr U kStatusIcons = 0x64B048;      // u8 by status bit: the icon's animation
constexpr U kCountX = 0x64B073;           // u8 by the party count: window 1's x
constexpr U kFormationWeights3 = 0x64B18C;  // u8 [formation * 3 + member]
constexpr U kFormationWeights2 = 0x64B184;  // u8 [formation * 2 + member]
constexpr U kActKinds = 0x65563C;         // u8 by the enemy's +0x8E: four 2-bit kinds
constexpr U kAbilities = 0x65C4D8;        // NameTable_Abilities: 24 bytes a record, byte 0 flags
constexpr U kMemberOffsets = 0x64DF70;    // BattleWin_MemberTargetOffsets: s8 x by (row * 4 + column) * 2
constexpr U kMemberRowY = 0x64DFC8;       // u8 by row * 2 (+0 for column 0..1, +1 otherwise)
constexpr U kEnemyDataY = 0x8C564F;       // the area's enemy data records (stride 0x8C) + 0x87
constexpr U kEnemyDataCount = 0x8C5652;   // + 0x8A
constexpr U kFieldActors = 0x903A8E;      // Field_ActorStates + 0xE, stride 0x8C
constexpr U kMemberPositions = 0x7E06E0;  // i32 x, z per member (event_ops_callees.h kMemberPositions)
constexpr U kPalettes = 0x80D380;         // 0x40 bytes a member slot
constexpr U kPlaceCells = 0x9045FD;       // u8, u8 per member (3 apart)
constexpr U kGrid18 = 0x904608;           // the 18 dwords (bytes by row * 4 + column), window 18's
constexpr U kGrid19 = 0x904620;           // window 19's

// --- the member's cells kept across the restore (BE2's, round twelve) -----------
constexpr U kKeptAp = 0x675ECC;           // u16: +0x9A
constexpr U kKeptStatus = 0x675ECE;       // u16: +0x90 without 0x4000
constexpr U kKeptFlags = 0x675ED0;        // u32: +0x130 without 0x18000
constexpr U kKeptFlags2 = 0x675ED4;       // u32: +0x134 & 0xFFF884FD

}  // namespace at
}  // namespace battle_e2
