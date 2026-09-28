// Internal to boss_sb.cpp and boss_sb_fuzz.cpp: the cells group BSB's boss
// functions touch that symbols.toml has no name for, and the callees nobody
// owns, by raw address. docs/boss_sb.md.
//
// Raw-address callees (the round's rebinding pass names them):
//   0x446DE0  (): 0x904AA0 = 5, 0x904AA1 = 1, 0x904AA2 = 0 - the end phase's
//             step 1; engine code nobody owns (docs/boss_h.md section 9).
//   0x446E00  (): the same with step 2; nobody owns it.
//   0x446E20  (): the same with step 3; nobody owns it.
#pragma once

#include <cstdint>

namespace boss_sb {
namespace at {

using U = std::uint32_t;

// --- the battle's cells (0x904AA0..0x904BA0) --------------------------------
constexpr U kPhase = 0x904AA0;            // u8: Battle_PhaseDispatch's phase
constexpr U kStep = 0x904AA1;             // u8: the phase's step
constexpr U kCountdown = 0x904AA5;        // u8: a cursor or counter of the phase (B07's event hook counts it down)
constexpr U kFight = 0x904AAA;            // u8: the event battle
constexpr U kPoseBits = 0x904AAD;         // u8: bit n per enemy slot n (BattleObj_StateInit reads it as a pose base)
constexpr U kMusicFlags = 0x904AE5;       // u8
constexpr U kBattleEnd = 0x904AE8;        // u8: bit 0 the loss, bit 1 the win, 4 and 8 set here
constexpr U kActor = 0x904B34;            // u8: the acting actor
constexpr U kCommandKind = 0x904B35;      // u8: the command's kind (4 read and written here)
constexpr U kCommand = 0x904B40;          // unsigned char *: the command; its word +2 the id
constexpr U kTarget = 0x904B44;           // u8: the target
constexpr U kTargetBlock = 0x904B50;      // unsigned char *: the target's block (battle_actions.md)
constexpr U kHookEnd = 0x904B64;          // BattleHook_End
constexpr U kHookExit = 0x904B68;         // BattleHook_Exit
constexpr U kHookEvent = 0x904B6C;        // BattleHook_Event
constexpr U kCurrentEnemy = 0x939AD8;     // the enemy BattleEnemy_RunAll is running (its object)

// --- the party's leader (ObjTrio + 0) ---------------------------------------
constexpr U kLeader = 0x802D40;           // ObjTrio: B07's event hook makes it Sprite_Current
constexpr U kLeaderHp = 0x802DD8;         // u16 +0x98: the leader's HP
constexpr U kLeaderTarget = 0x802E64;     // u8 +0x124: the member's target
constexpr U kLeaderCmdKind = 0x802E65;    // u8 +0x125: the member's command kind
constexpr U kLeaderCmdId = 0x802E66;      // u16 +0x126: the member's command id
constexpr U kLeaderFlags = 0x802E74;      // dword +0x134: bit 1 set and cleared by the event hooks

// --- the enemies' objects (0x93B960 + n * 0x128) -----------------------------
constexpr U kEnemy0 = 0x93B960;
constexpr U kEnemy1 = 0x93BA88;
constexpr U kEnemy0Hp = 0x93BA04;         // enemy 0's u16 +0xA4
constexpr U kEnemy0B0 = 0x93BA10;         // enemy 0's u16 +0xB0
constexpr U kEnemy1Hp = 0x93BB2C;         // enemy 1's u16 +0xA4
constexpr U kEnemy1B0 = 0x93BB38;         // enemy 1's u16 +0xB0
constexpr U kEnemy0Pose = 0x93B9B8;       // enemy 0's words +0x58 / +0x5A
constexpr U kEnemy1Pose = 0x93BAE0;       // enemy 1's words +0x58 / +0x5A
constexpr U kEnemy1Place = 0x93BABC;      // enemy 1's dwords +0x34 / +0x38 / +0x3C

// --- cells outside the battle bytes ------------------------------------------
constexpr U kWindow4Flag = 0x8031F3;      // window record 4's byte +3 (battle_actor_copies.md)
constexpr U kScriptVar3 = 0x903848;       // the movement script's variable 3 (field-modes.md)
constexpr U kMusicTrack = 0x904131;       // Music_Track
constexpr U kDrawPassFlags = 0x7E0918;    // Draw_PassFlags
constexpr U kLeaderPick = 0x92BF18;       // the byte Boss_SetByLeaderId writes; the field code at 0x532EB4 reads it
constexpr U kChapterRun = 0x8034E4;       // u8: the chapter run (MoveScript_Var7)
constexpr U kChapterStep = 0x8034E5;      // u8: its step
constexpr U kBannerText = 0x904EA0;       // B07's banner text buffer (16 bytes copied in)
constexpr U kBannerSource = 0x65D008;     // the .data text copied there (not read here)

// --- the callees nobody owns -------------------------------------------------
constexpr U kEndWin = 0x446DE0;           // () the end phase, step 1
constexpr U kEndOther = 0x446E00;         // () the end phase, step 2
constexpr U kEndThird = 0x446E20;         // () the end phase, step 3

}  // namespace at
}  // namespace boss_sb
