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
//   0x591810  (byte, byte): al, an item class (Item_UseFlags, ours since group TWO 2026-10-06; the harness's
//             standard set lists it).
#pragma once

#include <cstdint>

#include "bof3/symbols.gen.h"   // the constants below name their functions since 2026-10-01 (round twelve's debt 2): the same values, so the fuzz keys stand

namespace battle_e3 {
namespace at {

using U = std::uint32_t;

// --- raw callees of other groups (docs/battle_e3.md section 7) ---
constexpr ::bof3::image::CodeVa kTurnVelocityC{bof3::addr::Battle_TurnVectorC};    // (sprite) BE4
constexpr ::bof3::image::CodeVa kTurnVelocity18{bof3::addr::Battle_TurnVector18};   // (sprite) BE4
constexpr ::bof3::image::CodeVa kRollB9{bof3::addr::Battle_MemberReactRoll};           // () -> al, BE4
constexpr ::bof3::image::CodeVa kNameToText{bof3::addr::Battle_MemberNameToText};       // (member) BE4
constexpr ::bof3::image::CodeVa kBannerByPair{bof3::addr::BattleBanner_AddLine};     // (byte, byte) BE4
constexpr ::bof3::image::CodeVa kPass44FDE0{bof3::addr::BattleForm_ApplyStats};       // () BE5
constexpr ::bof3::image::CodeVa kPass453300{bof3::addr::Battle_RecalcStats};       // (actor) BE6
constexpr ::bof3::image::CodeVa kSecondPopup{bof3::addr::Battle_SetApPopup};      // (word, actor) BE6
constexpr ::bof3::image::CodeVa kItemClass{bof3::addr::Item_UseFlags};        // (byte, byte) -> al; group TWO's (2026-10-06), the value unchanged

// --- the battle bytes (0x904AA0..0x904BA0, the harness's battle frame) ---
constexpr ::bof3::image::Va kRoundFlags{0x904AA8};       // u16: bit 2 an action's end, 6, 7 (a roll), 11 (a cast's end), 13, 14
// kFight: now EventBattle_Id (symbols.toml). u8: the event battle (0 none)
constexpr ::bof3::image::Va kPartyCount{0x904AB0};       // u8: the members the party loops walk (ObjTrio holds three)
// kMembersUp: now Battle_PartyStanding (symbols.toml). u8: counted down as a member falls; 0 is the loss
// kEnemiesLeft: now Battle_EnemiesLeft (symbols.toml). u8: counted down as an enemy leaves; 0 is the win
// kBattleEnd: now Battle_EndFlags (symbols.toml). u8: bit 0 the loss, bit 1 the win
// kKindsSeen: now BattleMisc_BannerKinds (symbols.toml). u8: the message kinds seen this frame (battle_windows.md)
constexpr ::bof3::image::Va kActor{0x904B34};            // u8: the acting actor
constexpr ::bof3::image::Va kActKind{0x904B35};          // u8: the action's kind (1, 4, 5 read here)
constexpr ::bof3::image::Va kTarget{0x904B44};           // u8: the target (bits 6, 7 read)
constexpr ::bof3::image::Va kHitActor{0x904B54};         // u8: the object being hit's actor (+5), stored here
// kHitSprite: now BattleE3_HitSprite (symbols.toml). unsigned char *: the object being hit, stored here
constexpr ::bof3::image::Va kResult{0x904B60};           // unsigned char *: the result record (Field_State + 0x124 here)
constexpr ::bof3::image::Va kAbility{0x904B80};          // u16: the ability id (24-byte records at 0x65C4D8)
constexpr ::bof3::image::Va kItemCategory{0x904B81};     // u8: its high byte, 0x591810's first argument
// kCost: now BattleE3_Cost (symbols.toml). u8: the cost taken from an enemy's +0xA6
// kSoundSet: now BattleE3_SoundSet (symbols.toml). u8: Battle_LoadSoundByKey's set
// kFormActor: now BattleFlow_ActorAt (symbols.toml). u8: an actor 0x904AA8 bit 14 belongs to
constexpr ::bof3::image::Va kStatusOr{0x904B98};         // u16: or-ed into the hit member's +0x90, then zeroed
constexpr ::bof3::image::Va kStatusOr2{0x904B9A};        // u16: zeroed at the hit

// --- the rest ---
// kCurrentEnemy: now BattleE3_CurrentEnemy (symbols.toml). unsigned char *: the enemy BattleEnemy_RunAll runs
constexpr ::bof3::image::Va kStatCopy{0x939F80};         // 32 bytes: the hit member's +0xA0.. copied here
// kStatFlag: now BattleDamage_TargetDef (symbols.toml). u16: set to 1 under the member's +0x90 bit 11
// kEvadeBoost: now BattleE3_EvadeBoost (symbols.toml). u8: += 25 to 100 under +0x130 bit 0
constexpr ::bof3::image::Va kWindow4State{0x8031F3};     // u8: window record 4's +3 (= 2 as an enemy leaves)
constexpr ::bof3::image::Va kAbilityFlags0{0x65C4D8};    // the ability record's byte +0 (NameTable_Abilities; bits 2, 4)
constexpr ::bof3::image::Va kAbilityWord4{0x65C4DC};     // its word +4 (bit 11)
constexpr ::bof3::image::Va kAbilityFlags5{0x65C4DD};    // its byte +5 (bit 3)
constexpr U kAbilityStride = 24;
constexpr ::bof3::image::Va kEnemyData8B{0x8C5653};      // the area's enemy data record +0x8B (stride 0x8C) by the enemy's +0xF0
constexpr U kEnemyDataStride = 0x8C;
// kCharRecords: now CharacterRecords (symbols.toml). CharacterRecords, 0xA4 each, by a member's +0x148
constexpr U kCharStride = 0xA4;
// kParty: now ObjTrio (symbols.toml). ObjTrio: three members of 0x14C
constexpr U kPartyStride = 0x14C;
constexpr unsigned kPartyMax = 3;
// kTasks: now BattleTask_Slots (symbols.toml). BattleTask_Create's 48 slots of 0x84
constexpr U kTaskStride = 0x84;
constexpr unsigned kTaskCount = 48;
constexpr ::bof3::image::Va kSpecialOffsets{0x6699A0};   // s8 pairs by the character +0x89 (state 10's task position)

}  // namespace at
}  // namespace battle_e3
