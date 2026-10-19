// Internal to boss_sf.cpp and boss_sf_fuzz.cpp: the cells the 54 functions of
// group BSF touch that symbols.toml has no name for, the literals they store,
// and the callees nobody owns, by raw address. docs/boss_sf.md.
//
// Raw-address callees (the round's rebinding pass names them):
//   0x437450  (unsigned sound): Sound_PlayEffect(sound) unless its low word is
//             0xFFFF - an enemy state's sound (engine code nobody owns; read
//             2026-09-28, 0x437450..0x437461; boss_sa_callees.h kPlayCue).
//   0x4376A0  (): Sprite_Current +1 = 2, +2 = 0, 0x446FD0(+5), 0x939AD8's
//             +0x110 &= ~0x200, and +0x105 = 0 unless 0x904AA8 bit 6 - the
//             end of an enemy's action (nobody owns it; read 2026-09-28,
//             0x4376A0..0x4376EF).
//   0x4376F0  (): with 0x939AD8's +0x90 bit 3 and 0x5B93D2's al bit 0,
//             0x904AA8 |= 0x80 and BattleTask_Create(0, 2) (nobody owns it;
//             read 2026-09-28, 0x4376F0..0x43771A).
//   0x454A80  (object): releases every Field_Slots record whose +0xC is the
//             object (boss_sc_callees.h kSlotsReleaseFor; nobody owns it).
//   0x455290  (object, script): the first free Field_Slots record of eight
//             taken for the object with the script (boss_sc_callees.h
//             kSlotStart; nobody owns it); its al is not read here.
//   0x446DE0 / 0x446E00 / 0x446E20  (): the end phase's steps 1 (the win), 2
//             and 3 (boss_h_callees.h; the harness's standard set).
#pragma once

#include <cstdint>

#include "bof3/symbols.gen.h"   // bof3::addr: the constants below naming group BE6's functions (docs/battle_e6.md section 5)

// Rebound 2026-09-29 (round twelve group BE4, docs/battle_e4.md section 9): the constants here naming BE4's functions read
// bof3::addr::<Name>; the values are unchanged (the fuzz keys on them).

namespace boss_sf {
namespace at {

using U = std::uint32_t;

// --- the battle's cells (0x904AA0..0x904BA0, the harness's battle bytes) ---
constexpr U kPhase = 0x904AA0;            // u8: Battle_PhaseDispatch's phase (5: the battle's end)
constexpr U kRoundFlags = 0x904AA8;       // u8: the round flags' low byte (bit 2 set by the kinds' action ends)
constexpr U kRoundFlagsHi = 0x904AA9;     // u8: the round flags' high byte (bit 3 set by Myria's cost step)
// kFight: now BattleActions_EventBattle (symbols.toml). u8: the event battle (Boss_SetupTable's index)
// kScript: now BossSe_Script (symbols.toml). u8: the fights' script bits (set-up 27: bits 0..5)
constexpr U kOrderBefore = 0x904ACB;      // u8[]: the turn order 0x904ACC one before - indexed by the cursor
// kCursor: now BossSe_RoundSlot (symbols.toml). u8: the turn order's cursor (battle_actions.md)
// kBattleEnd: now BattleActions_BattleEnd (symbols.toml). u8: bit 1 the win; bit 2 set by set-up 27's event hook
// kBanner: now BattleMisc_BannerKinds (symbols.toml). u8: bit 1 holds the phases until phase 5 (battle_phases_callees.h)
constexpr U kActKind = 0x904B35;          // u8: the acting kind (4 an ability)
constexpr U kTarget = 0x904B44;           // u8: the battle's target
// kMyriaWait: now BossSa_EndCount (symbols.toml). u16: Myria's pending effect code (0x440630's third word; 3, 5, 8 read)
constexpr U kAbility = 0x904B80;          // u16: the action id in hand (24-byte records at 0x65C4D8)
// kCost: now BattleE3_Cost (symbols.toml). u8: the AP cost shown (battle_actions_callees.h kCostShown)
// kSoundSet: now BattleE3_SoundSet (symbols.toml). u8: Battle_LoadSoundByKey's set (battle_obj_states_callees.h)
constexpr U kTurn = 0x904B90;             // u32: the turn counter (battle_phases.md)
// kHookEnd: now BattleHook_End (symbols.toml). BattleHook_End
// kHookExit: now BattleHook_Exit (symbols.toml). BattleHook_Exit
// kHookEvent: now BattleHook_Event (symbols.toml). BattleHook_Event

// --- the ability records (NameTable_Abilities, 24 bytes each) -----------------
constexpr U kAbilityFlags4 = 0x65C4D8;    // + 24 * id: the byte Myria's action pick tests with 4
constexpr U kAbilityFlags8 = 0x65C4DD;    // + 24 * id: the byte states 7 and 8 test with 8
constexpr U kAbilityStride = 24;

// --- the enemies (0x93B960 + n * 0x128) and the running sprite ---------------
// kCurrentEnemy: now BattleE3_CurrentEnemy (symbols.toml). the enemy BattleEnemy_RunAll is running (its object)
constexpr U kEnemies = 0x93B960;          // enemy 0's object
constexpr U kEnemy1 = 0x93BA88;           // enemy 1's object
constexpr U kEnemy0Status = 0x93B9F2;     // enemy 0's +0x92 (u32; bit 0x4000 read by set-up 27)
// kEnemy1Status: now BossH_Enemy1Status (symbols.toml). enemy 1's +0x92 (u32)
constexpr U kEnemyData = 0x8C55C8;        // the area's enemy data records (stride 0x8C), by the enemy's +0xF0
constexpr U kEnemyDataStride = 0x8C;
// kOwner: now BattleE2_Owner (symbols.toml). the running task's owner (BattleTask_RunAll: the slot's +0x80)
constexpr U kTaskSlots = 0x93A000;        // BattleTask_Create's slots
constexpr U kTaskStride = 0x84;
constexpr unsigned kTaskCount = 48;

// --- the party (ObjTrio, stride 0x14C) ---------------------------------------
// kParty: now ObjTrio (symbols.toml).
constexpr U kPartyStride = 0x14C;

// --- other cells ------------------------------------------------------------
// kWindowUp: now BossSf_WindowUp (symbols.toml). u8: a window record's byte (0x803430's); the count waits while set
constexpr U kScriptVar3 = 0x903848;       // u8: movement-script variable 3 (area_w0a.md), set by set-up 28's end
// kChapterStep: now MoveScript_PartyRecords[0x65] (symbols.toml). u8: the chapter run's step
constexpr U kPoseSet = 0x8C5D80;          // the frame set set-up 27's end hook poses the party from
constexpr U kTextRow1 = 0x904D00;         // Text_Records[1] (battle_actions_callees.h): the count's digits
// kCountText0: now BossSf_CountText0 (symbols.toml). .data: the text pointer drawn before the count
// kCountText1: now BossSf_CountText1 (symbols.toml). .data: the text pointer drawn after it
// kCountFormat: now Boss26Fx_CountFormat (symbols.toml). Boss26Fx_CountFormat, the count's sprintf format
constexpr U kMyriaSlotScript = 0x64DD04;  // .data: the script Myria's entrance hands 0x455290 (not read)

// --- the callees nobody owns ------------------------------------------------
constexpr U kEnemySound = bof3::addr::Sound_PlayEffectUnlessNone;       // (sound) (BE3's since round twelve: the same value)
constexpr U kEnemyActEnd = bof3::addr::EnemyOp_EndAction;      // () (BE3's since round twelve: the same value)
constexpr U kEnemyActChance = bof3::addr::EnemyOp_RollBit80Task;   // () (BE3's since round twelve: the same value)
constexpr U kSlotsReleaseFor = bof3::addr::Field_SlotsReleaseOwner;  // 0x454A80 (object; BE6, 2026-09-29)
constexpr U kSlotStart = bof3::addr::Field_SlotStart;               // 0x455290 (object, script; BE6, 2026-09-29)
constexpr U kEndWin = bof3::addr::BattleEnd_EnterStep1;           // () the end phase, step 1
constexpr U kEndOther = bof3::addr::BattleEnd_EnterStep2;         // () step 2
constexpr U kEndThird = bof3::addr::BattleEnd_EnterStep3;         // () step 3

}  // namespace at
}  // namespace boss_sf
