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
constexpr ::bof3::image::CodeVa kMemberRecalc{bof3::addr::BattleParty_RecalcStats};     // BE3
constexpr ::bof3::image::CodeVa kMemberStatusSet{bof3::addr::Battle_RecalcStats};  // BE6
constexpr ::bof3::image::CodeVa kTurnVelocity{bof3::addr::Battle_TurnVectorC};     // BE4
constexpr ::bof3::image::CodeVa kPlayCue{bof3::addr::Sound_PlayEffectUnlessNone};          // BE3
constexpr ::bof3::image::CodeVa kEnemyTaskChance{bof3::addr::EnemyOp_RollBit80Task};  // BE3
constexpr ::bof3::image::CodeVa kEnemyOpEnd{bof3::addr::EnemyOp_EndAction};       // BE3
constexpr ::bof3::image::CodeVa kActorMayAct{bof3::addr::Battle_AutoTargetCheck};      // R4A's (round 14), ours

// --- the battle bytes (the harness's 0x904AA0..0x904BA0) ----------------------
constexpr ::bof3::image::Va kRoundFlags{0x904AA8};       // u16 / u32: the round flags (0x400, 0x4000, 0x8000 read here)
// kFight: now EventBattle_Id (symbols.toml). u8: the event battle, 0 none
// kTapCommand: now BattleE5_GeneRow (symbols.toml). u8: 0xFF none (battle_phases_callees.h kTapCommand)
constexpr ::bof3::image::Va kPartyCount{0x904AB0};       // u8 (read as a dword, the low byte): members in the fight
// kPartyIn: now Battle_PartyStanding (symbols.toml). u8: members counted in
// kEnemiesLeft: now Battle_EnemiesLeft (symbols.toml). u8
constexpr ::bof3::image::Va kActor{0x904B34};            // u8: the acting actor
constexpr ::bof3::image::Va kActKind{0x904B35};          // u8: the action's kind (1 attack, 2, 3, 4 an ability)
constexpr ::bof3::image::Va kTarget{0x904B44};           // u8: the target, 0xFF none
constexpr ::bof3::image::Va kMagicId{0x904B80};          // u16: the ability or spell id
constexpr ::bof3::image::Va kPickList{0x904B84};         // u8[3]: the list effect 9 compares its +0xB with
// kPickCount: now BattleE5_GeneCount (symbols.toml). u8: entries of that list
// kSavedCount: now BattleE2_SavedCount (symbols.toml). u8: the party count Accession kept (magic_s33.cpp)
// kPlace94: now BattleE2_Place94 (symbols.toml). u8, u8: copied into 0x9045FD / 0x9045FE
// kPlace95: now BattleE2_Place95 (symbols.toml).

// --- the party's formation (inventory_ops_callees.h kFormation) --------------
constexpr ::bof3::image::Va kFormation{0x904060};        // u8 (read as a dword, the low byte)

// --- the party (ObjTrio, stride 0x14C) and its backup (0x939AE0, the same
// stride: battle_turn_steps_callees.h kCopies) ------------------------------------
// kParty: now ObjTrio (symbols.toml).
constexpr U kPartyStride = 0x14C;
constexpr ::bof3::image::Va kBackup{0x939AE0};
constexpr ::bof3::image::Va kBackupFlags{0x939C14};      // kBackup + 0x134
constexpr ::bof3::image::Va kBackupBit40{0x939B71};      // kBackup + 0x91 (bit 0x40 tested)

// --- the enemies' objects (0x93B960, stride 0x128) -----------------------------
// kEnemies: now Battle_Enemies (symbols.toml).
constexpr U kEnemyStride = 0x128;

// --- the task slots (0x93A000, stride 0x84; +0x80 the owner) --------------------
constexpr ::bof3::image::Va kTasks{0x93A000};
constexpr U kTaskStride = 0x84;
constexpr ::bof3::image::Va kOwner{0x93B940};            // the running slot's owner (a pointer)
// kEnemyCurrent: now BattleE3_CurrentEnemy (symbols.toml). the current enemy (a pointer)

// --- the window records (WindowRecords 0x803160, stride 0x24) ------------------
// kWindow1Count: now Window1_ParamA (symbols.toml). record 1 (0x803184) +0xA: the party count
constexpr ::bof3::image::Va kWindow1X{0x803188};         // record 1 +4 (u16): x by the count
constexpr ::bof3::image::Va kPartyGauges{0x80333F};      // record 13 + i (0x803334 + 0x24 i) +0xB / +0xC: HP / AP gauge bytes
constexpr ::bof3::image::Va kPartyValues{0x803348};      // the same records +0x14 / +0x16: u16 HP, u16 AP
// kWindow18: now Window18 (symbols.toml). record 18: +0 in use, +4 / +6 x, y, +0x12 a row base (s16), +0x1E its copy
// kWindow19: now Window19 (symbols.toml). record 19: the same fields
// kWindow21: now Window21 (symbols.toml). record 21: +0 in use, +4 / +6 x, y

// --- the pointer the sprite pose pool lives at while a task draws --------------
// kAnimSet: now BattleE2_AnimSet (symbols.toml).
constexpr ::bof3::image::Va kAnimSetBattle{0x8C5D80};
constexpr ::bof3::image::Va kAnimSetField{0x8B3580};

// --- tables read in place (.data) ----------------------------------------------
constexpr ::bof3::image::Va kStatusIcons{0x64B048};      // u8 by status bit: the icon's animation
constexpr ::bof3::image::Va kCountX{0x64B073};           // u8 by the party count: window 1's x
constexpr ::bof3::image::Va kFormationWeights3{0x64B18C};  // u8 [formation * 3 + member]
constexpr ::bof3::image::Va kFormationWeights2{0x64B184};  // u8 [formation * 2 + member]
constexpr ::bof3::image::Va kActKinds{0x65563C};         // u8 by the enemy's +0x8E: four 2-bit kinds
constexpr ::bof3::image::Va kAbilities{0x65C4D8};        // NameTable_Abilities: 24 bytes a record, byte 0 flags
// kMemberOffsets: now BattleWin_MemberTargetOffsets (symbols.toml). BattleWin_MemberTargetOffsets: s8 x by (row * 4 + column) * 2
constexpr ::bof3::image::Va kMemberRowY{0x64DFC8};       // u8 by row * 2 (+0 for column 0..1, +1 otherwise)
constexpr ::bof3::image::Va kEnemyDataY{0x8C564F};       // the area's enemy data records (stride 0x8C) + 0x87
constexpr ::bof3::image::Va kEnemyDataCount{0x8C5652};   // + 0x8A
// kFieldActors: now Field_ActorStates[0xe] (symbols.toml). Field_ActorStates + 0xE, stride 0x8C
constexpr ::bof3::image::Va kMemberPositions{0x7E06E0};  // i32 x, z per member (event_ops_callees.h kMemberPositions)
// kPalettes: now Gfx_ClutStripSource[0xf00] (symbols.toml). 0x40 bytes a member slot
constexpr ::bof3::image::Va kPlaceCells{0x9045FD};       // u8, u8 per member (3 apart)
constexpr ::bof3::image::Va kGrid18{0x904608};           // the 18 dwords (bytes by row * 4 + column), window 18's
constexpr ::bof3::image::Va kGrid19{0x904620};           // window 19's

// --- the member's cells kept across the restore (BE2's, round twelve) -----------
// kKeptAp: now BattleE2_KeptAp (symbols.toml). u16: +0x9A
// kKeptStatus: now BattleE2_KeptStatus (symbols.toml). u16: +0x90 without 0x4000
// kKeptFlags: now BattleE2_KeptFlags (symbols.toml). u32: +0x130 without 0x18000
// kKeptFlags2: now BattleE2_KeptFlags2 (symbols.toml). u32: +0x134 & 0xFFF884FD

}  // namespace at
}  // namespace battle_e2
