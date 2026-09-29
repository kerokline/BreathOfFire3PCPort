// Internal to boss_h.cpp and boss_h_fuzz.cpp: the cells and tables the 20
// shared boss helpers touch that symbols.toml has no name for, and the callees
// nobody owns, by raw address. docs/boss_h.md.
//
// Raw-address callees (the round's rebinding pass names them):
//   0x446DE0  (): 0x904AA0 = 5, 0x904AA1 = 1, 0x904AA2 = 0 - the end phase's
//             step 1 (BattleEnd_Steps: the win); engine code nobody owns
//             (read 2026-09-28, 0x446DE0..0x446DF5).
//   0x446E00  (): the same with step 2 (the other way out); nobody owns it.
#pragma once

#include <cstdint>

#include "bof3/symbols.gen.h"

// Rebound 2026-09-29 (round twelve group BE4, docs/battle_e4.md section 9): the constants here naming BE4's functions read
// bof3::addr::<Name>; the values are unchanged (the fuzz keys on them).

namespace boss_h {
namespace at {

using U = std::uint32_t;

// --- the battle's cells -----------------------------------------------------
constexpr U kCurrentEnemy = 0x939AD8;     // the enemy EnemyRunAll is running (its object)
constexpr U kFight = 0x904AAA;            // u8: the event battle
constexpr U kBattleEnd = 0x904AE8;        // u8: bit 1 the win
constexpr U kTarget = 0x904B44;           // u8: the battle's target
constexpr U kChapterStep = 0x8034E5;      // u8: the chapter run's step
constexpr U kFlagBits = 0x929ED0;         // unsigned char *: the chapter's flag bits
constexpr U kPacketNext = 0x7E0670;       // Gfx_PacketNext

// --- the party's leader (ObjTrio + 0) ---------------------------------------
constexpr U kLeaderX = 0x802D74;          // +0x34, 16.16
constexpr U kLeaderZ = 0x802D78;          // +0x38
constexpr U kLeaderY = 0x802D7C;          // +0x3C
constexpr U kLeaderGround = 0x802D7E;     // +0x3E: the word AreaMap_Elevation answers is stored here
constexpr U kLeaderId = 0x802DC9;         // +0x89: the character id
constexpr U kLeaderTarget = 0x802E64;     // +0x124: the member's target
constexpr U kLeaderFlags = 0x802E74;      // +0x134: bit 1 copies its +0x3C to 0x939B1C
constexpr U kCameraY = 0x939B1C;          // written from the leader's +0x3C (what reads it is not traced)

// --- the enemies' objects (0x93B960 + n * 0x128) that BossMap_UpdateFromEnemies reads
constexpr U kEnemy0X = 0x93B994;          // enemy 0's +0x34 / +0x38, and the word +0x3E
constexpr U kEnemy0Z = 0x93B998;
constexpr U kEnemy0Ground = 0x93B99E;
constexpr U kEnemy1 = 0x93BA88;           // enemy 1's object; +0x93 at 0x93BB1B
constexpr U kEnemy1Status = 0x93BB1A;     // its +0x92 (the dword read; byte +0x93 tested)
constexpr U kEnemy2 = 0x93BBB0;           // enemy 2's object
constexpr U kEnemy2Status = 0x93BC42;     // its +0x92

// --- the loaded area block --------------------------------------------------
constexpr U kMapHeader = 0x8CB580;        // AreaMap_Header: byte +0 the grid's width
constexpr U kMapCorners = 0x8CB5B0;       // AreaMap_Corners: a dword per cell

// --- a byte Boss_SetByLeaderId writes (0x2C..0x2F by the leader's id); the
// field code at 0x532EB4 reads it - what it selects was not read.
constexpr U kLeaderPick = 0x92BF18;

// --- the callees nobody owns ------------------------------------------------
constexpr U kEndWin = bof3::addr::BattleEnd_EnterStep1;           // () the end phase, step 1
constexpr U kEndOther = bof3::addr::BattleEnd_EnterStep2;         // () the end phase, step 2

}  // namespace at
}  // namespace boss_h
