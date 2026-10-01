// Internal to battle_e3.cpp and battle_e3_fuzz.cpp: the cells group BE3's 49
// functions touch that symbols.toml has no name for, and the callees another
// group of round twelve's wave one owns, by raw address (the round's
// rebinding pass names them once both have merged). docs/battle_e3.md.
//
// Raw-address callees (each read 2026-09-29, capstone):
//   0x446770  (sprite): by the sprite's pose byte +8 (1, 2, 3) the velocity
//             pair +0xC / +0x10 turned a quarter or a half (BE4's).
//   0x4467C0  (sprite): the same for the pair +0x18 / +0x1C (BE4's).
//   0x446810  (): al 0 or 1 - 1 under Field_State +0x130 bit 15, else a
//             Rand() % 100 roll below +0xB9, behind six gates answering 0
//             (0x904AA8 bit 6, which it clears; +0x130 bit 0; the actor
//             0x904B34 below 3; +0x90 & 0x4864; the ability 0xA1 for kind 4;
//             0x904B8E without +0x134 bit 4) (BE4's).
//   0x44A910  (member byte): the member's name into Text_Records (BE4's).
//   0x44AA90  (byte, byte): a banner by the two bytes' message (BE4's).
//   0x44FDE0  (): BE5's (not read here).
//   0x453300  (actor byte): BE6's (for a member, 0x453560 over its record).
//   0x453EB0  (word, actor byte): a second damage popup task - the sibling of
//             Battle_SetDamagePopup (BE6's).
//   0x591810  (byte, byte): al, an item class (nobody's; the harness's
//             standard set lists it).
#pragma once

#include <cstdint>

#include "bof3/symbols.gen.h"   // the constants below name their functions since 2026-10-01 (round twelve's debt 2): the same values, so the fuzz keys stand

namespace battle_e3 {
namespace at {

using U = std::uint32_t;

// --- raw callees of other groups (docs/battle_e3.md section 7) ---
constexpr U kTurnVelocityC = bof3::addr::Battle_TurnVectorC;    // (sprite) BE4
constexpr U kTurnVelocity18 = bof3::addr::Battle_TurnVector18;   // (sprite) BE4
constexpr U kRollB9 = bof3::addr::Battle_MemberReactRoll;           // () -> al, BE4
constexpr U kNameToText = bof3::addr::Battle_MemberNameToText;       // (member) BE4
constexpr U kBannerByPair = bof3::addr::BattleBanner_AddLine;     // (byte, byte) BE4
constexpr U kPass44FDE0 = bof3::addr::BattleForm_ApplyStats;       // () BE5
constexpr U kPass453300 = bof3::addr::Battle_RecalcStats;       // (actor) BE6
constexpr U kSecondPopup = bof3::addr::Battle_SetApPopup;      // (word, actor) BE6
constexpr U kItemClass = 0x591810;        // (byte, byte) -> al, nobody's

// --- the battle bytes (0x904AA0..0x904BA0, the harness's battle frame) ---
constexpr U kRoundFlags = 0x904AA8;       // u16: bit 2 an action's end, 6, 7 (a roll), 11 (a cast's end), 13, 14
constexpr U kFight = 0x904AAA;            // u8: the event battle (0 none)
constexpr U kPartyCount = 0x904AB0;       // u8: the members the party loops walk (ObjTrio holds three)
constexpr U kMembersUp = 0x904AB1;        // u8: counted down as a member falls; 0 is the loss
constexpr U kEnemiesLeft = 0x904AB3;      // u8: counted down as an enemy leaves; 0 is the win
constexpr U kBattleEnd = 0x904AE8;        // u8: bit 0 the loss, bit 1 the win
constexpr U kKindsSeen = 0x904AE9;        // u8: the message kinds seen this frame (battle_windows.md)
constexpr U kActor = 0x904B34;            // u8: the acting actor
constexpr U kActKind = 0x904B35;          // u8: the action's kind (1, 4, 5 read here)
constexpr U kTarget = 0x904B44;           // u8: the target (bits 6, 7 read)
constexpr U kHitActor = 0x904B54;         // u8: the object being hit's actor (+5), stored here
constexpr U kHitSprite = 0x904B5C;        // unsigned char *: the object being hit, stored here
constexpr U kResult = 0x904B60;           // unsigned char *: the result record (Field_State + 0x124 here)
constexpr U kAbility = 0x904B80;          // u16: the ability id (24-byte records at 0x65C4D8)
constexpr U kItemCategory = 0x904B81;     // u8: its high byte, 0x591810's first argument
constexpr U kCost = 0x904B88;             // u8: the cost taken from an enemy's +0xA6
constexpr U kSoundSet = 0x904B8D;         // u8: Battle_LoadSoundByKey's set
constexpr U kFormActor = 0x904B8A;        // u8: an actor 0x904AA8 bit 14 belongs to
constexpr U kStatusOr = 0x904B98;         // u16: or-ed into the hit member's +0x90, then zeroed
constexpr U kStatusOr2 = 0x904B9A;        // u16: zeroed at the hit

// --- the rest ---
constexpr U kCurrentEnemy = 0x939AD8;     // unsigned char *: the enemy BattleEnemy_RunAll runs
constexpr U kStatCopy = 0x939F80;         // 32 bytes: the hit member's +0xA0.. copied here
constexpr U kStatFlag = 0x939F86;         // u16: set to 1 under the member's +0x90 bit 11
constexpr U kEvadeBoost = 0x939F9B;       // u8: += 25 to 100 under +0x130 bit 0
constexpr U kWindow4State = 0x8031F3;     // u8: window record 4's +3 (= 2 as an enemy leaves)
constexpr U kAbilityFlags0 = 0x65C4D8;    // the ability record's byte +0 (NameTable_Abilities; bits 2, 4)
constexpr U kAbilityWord4 = 0x65C4DC;     // its word +4 (bit 11)
constexpr U kAbilityFlags5 = 0x65C4DD;    // its byte +5 (bit 3)
constexpr U kAbilityStride = 24;
constexpr U kEnemyData8B = 0x8C5653;      // the area's enemy data record +0x8B (stride 0x8C) by the enemy's +0xF0
constexpr U kEnemyDataStride = 0x8C;
constexpr U kCharRecords = 0x903A70;      // CharacterRecords, 0xA4 each, by a member's +0x148
constexpr U kCharStride = 0xA4;
constexpr U kParty = 0x802D40;            // ObjTrio: three members of 0x14C
constexpr U kPartyStride = 0x14C;
constexpr unsigned kPartyMax = 3;
constexpr U kTasks = 0x93A000;            // BattleTask_Create's 48 slots of 0x84
constexpr U kTaskStride = 0x84;
constexpr unsigned kTaskCount = 48;
constexpr U kSpecialOffsets = 0x6699A0;   // s8 pairs by the character +0x89 (state 10's task position)

}  // namespace at
}  // namespace battle_e3
