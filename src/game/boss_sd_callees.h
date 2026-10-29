// Internal to boss_sd.cpp and boss_sd_fuzz.cpp: the cells group BSD's
// functions touch that symbols.toml has no name for, the literals they store
// (the hooks they install, the tables they point an enemy at), and the
// callees nobody owns, by raw address. docs/boss_sd.md.
//
// Raw-address callees (the round's rebinding pass names them):
//   0x446DE0  (): 0x904AA0 = 5, 0x904AA1 = 1, 0x904AA2 = 0 - the end phase's
//             step 1 (the win); engine code nobody owns (boss_h_callees.h).
//   0x446E00  (): the same with step 2 (the other way out); nobody owns it.
//   0x446E20  (): the same with step 3; nobody owns it (the harness's
//             standard set lists all three).
#pragma once

#include <cstdint>

#include "bof3/symbols.gen.h"

// Rebound 2026-09-29 (round twelve group BE4, docs/battle_e4.md section 9): the constants here naming BE4's functions read
// bof3::addr::<Name>; the values are unchanged (the fuzz keys on them).
// Rebound 2026-09-28 (round eleven's cleanup, docs/round-11-cleanup.md item 2):
// every constant here whose target has a name in symbols.toml reads
// bof3::addr::<Name>. The values are unchanged - the fuzz keys on them.

namespace boss_sd {
namespace at {

using U = std::uint32_t;

// --- the battle's cells -----------------------------------------------------
// kCurrentEnemy: now BattleE3_CurrentEnemy (symbols.toml). the enemy BattleEnemy_RunAll is running (its object)
// kHookEnd: now BattleHook_End (symbols.toml). BattleHook_End
// kHookExit: now BattleHook_Exit (symbols.toml). BattleHook_Exit
// kHookEvent: now BattleHook_Event (symbols.toml). BattleHook_Event
// kWhoFell: now BossSe_Script (symbols.toml). u8: set-ups 18..20 keep here which of enemies 1 (bit 1) and 2 (bit 0) fell
// kBattleEnd: now Battle_EndFlags (symbols.toml). u8: bit 0 the loss, bit 1 the win
constexpr ::bof3::image::Va kActKind{0x904B35};          // u8: the action kind (1 attack, 4 ability, 5 item)
// kChapterStep: now MoveScript_PartyRecords[0x65] (symbols.toml). u8: the chapter run's step
// kFlagBits: now Chapter_FlagRow (symbols.toml). unsigned char *: the chapter's flag bits

// The field's move-script counter 0 (0x903848..0x90384B are MoveScript's four
// counters, symbols.toml at 0x57C460): set-ups 17 and 21's end hooks set it on
// the win.
constexpr ::bof3::image::Va kCounter0{0x903848};

// Four words set-ups 18..20's end hooks save as a fight ends - enemy 1's HP
// (0x939A18), enemy 2's (0x939A16), enemy 0's +0xA4 (0x939A1A) and +0xA6
// (0x939A14) - and the area-79 kinds' entrances read back when a chapter
// flag is set (kinds 21..23 on flags 0x35..0x37: +0xA4 / +0xA6 from 0x939A1A /
// 0x939A14; kind 26 on 0x1E or 0x23: +0xA4 from 0x939A18 or 0x939A16 by its
// +5). What the words carry between the fights is read from the code only.
// kKeptA6: now BossSd_KeptA6 (symbols.toml).
// kKeptHp2: now BossSd_KeptHp2 (symbols.toml).
// kKeptHp1: now BossSd_KeptHp1 (symbols.toml).
// kKeptA4: now BossSd_KeptA4 (symbols.toml).

// --- the enemies' objects (0x93B960 + n * 0x128) the set-ups read -----------
// kEnemy0: now Battle_Enemies (symbols.toml).
// kEnemy1: now Battle_Enemies[1] (symbols.toml).
// kEnemy2: now Battle_Enemies[2] (symbols.toml).
constexpr ::bof3::image::Va kEnemy0Status{0x93B9F2};     // enemy 0's +0x92 (a dword read; +0x93 bit 0x40 tested)
// kEnemy1Status: now BossH_Enemy1Status (symbols.toml). enemy 1's +0x92
// kEnemy2Status: now BossH_Enemy2Status (symbols.toml). enemy 2's +0x92
constexpr ::bof3::image::Va kEnemy0Hp{0x93BA04};         // enemy 0's +0xA4
constexpr ::bof3::image::Va kEnemy0A6{0x93BA06};         // enemy 0's +0xA6
// kEnemy1Hp: now BossSb_Enemy1Hp (symbols.toml). enemy 1's +0xA4
// kEnemy2Hp: now BossSd_Enemy2Hp (symbols.toml). enemy 2's +0xA4
constexpr ::bof3::image::Va kEnemy0X{0x93B994};          // enemy 0's +0x34 / +0x38, and the word +0x3E
constexpr ::bof3::image::Va kEnemy0Z{0x93B998};
// kEnemy0Ground: now Battle_Enemies[0].height_whole (symbols.toml).

// --- the party (ObjTrio, stride 0x14C) -----------------------------------------
// kPartyFlags: now ObjTrio[0].status (symbols.toml). member 0's +0x90 (bit 0x10 set by set-up 21's event hook)
constexpr U kPartyStride = 0x14C;
constexpr U kEnemyStride = 0x128;
// kLeaderX: now ObjTrio[0].x (symbols.toml). the leader's +0x34
// kLeaderZ: now ObjTrio[0].z (symbols.toml). +0x38
// kLeaderGround: now ObjTrio[0].height_whole (symbols.toml). +0x3E

// --- the literals the set-ups store (their own functions and BH's) ----------------
constexpr ::bof3::image::CodeVa kBareRetZero{bof3::addr::BareRetZero};      // BareRetZero (BH)
constexpr ::bof3::image::CodeVa kExitActor0Bit40{bof3::addr::BossHook_ExitActor0Bit40};  // BossHook_ExitActor0Bit40 (BH)

// --- the callees nobody owns -------------------------------------------------------
constexpr ::bof3::image::CodeVa kEndWin{bof3::addr::BattleEnd_EnterStep1};           // () the end phase, step 1
constexpr ::bof3::image::CodeVa kEndOther{bof3::addr::BattleEnd_EnterStep2};         // () the end phase, step 2
constexpr ::bof3::image::CodeVa kEndThird{bof3::addr::BattleEnd_EnterStep3};         // () the end phase, step 3

}  // namespace at
}  // namespace boss_sd
