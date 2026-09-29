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
// Rebound 2026-09-28 (round eleven's cleanup, docs/round-11-cleanup.md item 2):
// every constant here whose target has a name in symbols.toml reads
// bof3::addr::<Name>. The values are unchanged - the fuzz keys on them.

namespace boss_sd {
namespace at {

using U = std::uint32_t;

// --- the battle's cells -----------------------------------------------------
constexpr U kCurrentEnemy = 0x939AD8;     // the enemy BattleEnemy_RunAll is running (its object)
constexpr U kHookEnd = 0x904B64;          // BattleHook_End
constexpr U kHookExit = 0x904B68;         // BattleHook_Exit
constexpr U kHookEvent = 0x904B6C;        // BattleHook_Event
constexpr U kWhoFell = 0x904AAD;          // u8: set-ups 18..20 keep here which of enemies 1 (bit 1) and 2 (bit 0) fell
constexpr U kBattleEnd = 0x904AE8;        // u8: bit 0 the loss, bit 1 the win
constexpr U kActKind = 0x904B35;          // u8: the action kind (1 attack, 4 ability, 5 item)
constexpr U kChapterStep = 0x8034E5;      // u8: the chapter run's step
constexpr U kFlagBits = 0x929ED0;         // unsigned char *: the chapter's flag bits

// The field's move-script counter 0 (0x903848..0x90384B are MoveScript's four
// counters, symbols.toml at 0x57C460): set-ups 17 and 21's end hooks set it on
// the win.
constexpr U kCounter0 = 0x903848;

// Four words set-ups 18..20's end hooks save as a fight ends - enemy 1's HP
// (0x939A18), enemy 2's (0x939A16), enemy 0's +0xA4 (0x939A1A) and +0xA6
// (0x939A14) - and the area-79 kinds' entrances read back when a chapter
// flag is set (kinds 21..23 on flags 0x35..0x37: +0xA4 / +0xA6 from 0x939A1A /
// 0x939A14; kind 26 on 0x1E or 0x23: +0xA4 from 0x939A18 or 0x939A16 by its
// +5). What the words carry between the fights is read from the code only.
constexpr U kKeptA6 = 0x939A14;
constexpr U kKeptHp2 = 0x939A16;
constexpr U kKeptHp1 = 0x939A18;
constexpr U kKeptA4 = 0x939A1A;

// --- the enemies' objects (0x93B960 + n * 0x128) the set-ups read -----------
constexpr U kEnemy0 = 0x93B960;
constexpr U kEnemy1 = 0x93BA88;
constexpr U kEnemy2 = 0x93BBB0;
constexpr U kEnemy0Status = 0x93B9F2;     // enemy 0's +0x92 (a dword read; +0x93 bit 0x40 tested)
constexpr U kEnemy1Status = 0x93BB1A;     // enemy 1's +0x92
constexpr U kEnemy2Status = 0x93BC42;     // enemy 2's +0x92
constexpr U kEnemy0Hp = 0x93BA04;         // enemy 0's +0xA4
constexpr U kEnemy0A6 = 0x93BA06;         // enemy 0's +0xA6
constexpr U kEnemy1Hp = 0x93BB2C;         // enemy 1's +0xA4
constexpr U kEnemy2Hp = 0x93BC54;         // enemy 2's +0xA4
constexpr U kEnemy0X = 0x93B994;          // enemy 0's +0x34 / +0x38, and the word +0x3E
constexpr U kEnemy0Z = 0x93B998;
constexpr U kEnemy0Ground = 0x93B99E;

// --- the party (ObjTrio, stride 0x14C) -----------------------------------------
constexpr U kPartyFlags = 0x802DD0;       // member 0's +0x90 (bit 0x10 set by set-up 21's event hook)
constexpr U kPartyStride = 0x14C;
constexpr U kEnemyStride = 0x128;
constexpr U kLeaderX = 0x802D74;          // the leader's +0x34
constexpr U kLeaderZ = 0x802D78;          // +0x38
constexpr U kLeaderGround = 0x802D7E;     // +0x3E

// --- the literals the set-ups store (their own functions and BH's) ----------------
constexpr U kBareRetZero = bof3::addr::BareRetZero;      // BareRetZero (BH)
constexpr U kExitActor0Bit40 = bof3::addr::BossHook_ExitActor0Bit40;  // BossHook_ExitActor0Bit40 (BH)

// --- the callees nobody owns -------------------------------------------------------
constexpr U kEndWin = 0x446DE0;           // () the end phase, step 1
constexpr U kEndOther = 0x446E00;         // () the end phase, step 2
constexpr U kEndThird = 0x446E20;         // () the end phase, step 3

}  // namespace at
}  // namespace boss_sd
