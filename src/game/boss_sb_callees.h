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

#include "bof3/symbols.gen.h"

// Rebound 2026-09-29 (round twelve group BE4, docs/battle_e4.md section 9): the constants here naming BE4's functions read
// bof3::addr::<Name>; the values are unchanged (the fuzz keys on them).

namespace boss_sb {
namespace at {

using U = std::uint32_t;

// --- the battle's cells (0x904AA0..0x904BA0) --------------------------------
constexpr U kPhase = 0x904AA0;            // u8: Battle_PhaseDispatch's phase
// kStep: now BattleActions_Step (symbols.toml). u8: the phase's step
// kCountdown: now BossSb_Countdown (symbols.toml). u8: a cursor or counter of the phase (B07's event hook counts it down)
// kFight: now EventBattle_Id (symbols.toml). u8: the event battle
// kPoseBits: now BossSe_Script (symbols.toml). u8: bit n per enemy slot n (BattleObj_StateInit reads it as a pose base)
// kMusicFlags: now EventBattle_Flags (symbols.toml). u8
// kBattleEnd: now Battle_EndFlags (symbols.toml). u8: bit 0 the loss, bit 1 the win, 4 and 8 set here
constexpr U kActor = 0x904B34;            // u8: the acting actor
constexpr U kCommandKind = 0x904B35;      // u8: the command's kind (4 read and written here)
// kCommand: now BattleSprites_Action (symbols.toml). unsigned char *: the command; its word +2 the id
constexpr U kTarget = 0x904B44;           // u8: the target
// kTargetBlock: now BattleSprites_HitBlock (symbols.toml). unsigned char *: the target's block (battle_actions.md)
// kHookEnd: now BattleHook_End (symbols.toml). BattleHook_End
// kHookExit: now BattleHook_Exit (symbols.toml). BattleHook_Exit
// kHookEvent: now BattleHook_Event (symbols.toml). BattleHook_Event
// kCurrentEnemy: now BattleE3_CurrentEnemy (symbols.toml). the enemy BattleEnemy_RunAll is running (its object)

// --- the party's leader (ObjTrio + 0) ---------------------------------------
// kLeader: now ObjTrio (symbols.toml). ObjTrio: B07's event hook makes it Sprite_Current
// kLeaderHp: now ObjTrio[0x98] (symbols.toml). u16 +0x98: the leader's HP
// kLeaderTarget: now ObjTrio[0x124] (symbols.toml). u8 +0x124: the member's target
// kLeaderCmdKind: now ObjTrio[0x125] (symbols.toml). u8 +0x125: the member's command kind
// kLeaderCmdId: now ObjTrio[0x126] (symbols.toml). u16 +0x126: the member's command id
// kLeaderFlags: now ObjTrio[0x134] (symbols.toml). dword +0x134: bit 1 set and cleared by the event hooks

// --- the enemies' objects (0x93B960 + n * 0x128) -----------------------------
constexpr U kEnemy0 = 0x93B960;
constexpr U kEnemy1 = 0x93BA88;
constexpr U kEnemy0Hp = 0x93BA04;         // enemy 0's u16 +0xA4
constexpr U kEnemy0B0 = 0x93BA10;         // enemy 0's u16 +0xB0
// kEnemy1Hp: now BossSb_Enemy1Hp (symbols.toml). enemy 1's u16 +0xA4
// kEnemy1B0: now BossSb_Enemy1B0 (symbols.toml). enemy 1's u16 +0xB0
// kEnemy0Pose: now BossSb_Enemy0Pose (symbols.toml). enemy 0's words +0x58 / +0x5A
// kEnemy1Pose: now BossSb_Enemy1Pose (symbols.toml). enemy 1's words +0x58 / +0x5A
// kEnemy1Place: now BossSb_Enemy1Place (symbols.toml). enemy 1's dwords +0x34 / +0x38 / +0x3C

// --- cells outside the battle bytes ------------------------------------------
constexpr U kWindow4Flag = 0x8031F3;      // window record 4's byte +3 (battle_actor_copies.md)
constexpr U kScriptVar3 = 0x903848;       // the movement script's variable 3 (field-modes.md)
// kMusicTrack: now Music_Track (symbols.toml). Music_Track
// kDrawPassFlags: now Draw_PassFlags (symbols.toml). Draw_PassFlags
// kLeaderPick: now BossH_LeaderPick (symbols.toml). the byte Boss_SetByLeaderId writes; the field code at 0x532EB4 reads it
// kChapterRun: now MoveScript_Var7 (symbols.toml). u8: the chapter run (MoveScript_Var7)
// kChapterStep: now MoveScript_PartyRecords[0x65] (symbols.toml). u8: its step
constexpr U kBannerText = 0x904EA0;       // B07's banner text buffer (16 bytes copied in)
constexpr U kBannerSource = 0x65D008;     // the .data text copied there (not read here)

// --- the callees nobody owns -------------------------------------------------
constexpr U kEndWin = bof3::addr::BattleEnd_EnterStep1;           // () the end phase, step 1
constexpr U kEndOther = bof3::addr::BattleEnd_EnterStep2;         // () the end phase, step 2
constexpr U kEndThird = bof3::addr::BattleEnd_EnterStep3;         // () the end phase, step 3

}  // namespace at
}  // namespace boss_sb
