// Internal to battle_sprites.cpp and battle_sprites_fuzz.cpp: every call the
// group's functions make, through pointers, so that the start-up fuzz can
// stand recording functions in for them - for the originals' copies and for
// ours alike. The callees this module implements are called through the same
// pointers, so each function is tested alone. docs/battle_sprites.md.
//
// Calls into other groups' functions (round seven: BB, BE, BF) and into
// functions nobody owns go through raw addresses here, never through a
// symbols.toml name (docs/takeover-queue-round7.md, "The rule for calls
// across groups"). Names already bound to our code (Stat_AddClamped,
// Sound_PlayEffect, ...) are called through their header names.
#pragma once

#include <cstdint>
#include "bof3/symbols.gen.h"

namespace battle_sprites {

// The addresses the group reads or writes that symbols.toml does not name
// (docs/battle_sprites.md, section 2).
namespace at {
// kParty: now ObjTrio (symbols.toml). ObjTrio: three 0x14C-byte party records, actor 0..2
constexpr std::uint32_t kPartyStride = 0x14C;
// kEnemies: now Battle_Enemies (symbols.toml). eight 0x128-byte enemy records, actor 3..10 (index actor - 3)
constexpr std::uint32_t kEnemyStride = 0x128;
constexpr ::bof3::image::Va kEnemyData{0x8C55C8};      // the loaded enemy data, 0x8C bytes per id: +0 the 12-byte name
constexpr std::uint32_t kEnemyDataStride = 0x8C;
// kEncounter: now Encounter_Slots (symbols.toml). eight 12-byte records: +0 used, +1 id, +4 x, +8 z
// kPopups: now BattleTask_Slots (symbols.toml). 0x84-byte records (0x435180 hands out a free one)
constexpr std::uint32_t kPopupStride = 0x84;
// kWindows: now WindowRecords (symbols.toml). 0x24-byte window records (0x59E2D0 claims one)
constexpr std::uint32_t kWindowStride = 0x24;
// kAction: now BattleSprites_Action (symbols.toml). pointer: the current action; its u16 at +2 is the action id
constexpr ::bof3::image::Va kTarget{0x904B44};         // byte: the target actor, or 0x40 / 0x80 / 0xC0 a side
constexpr ::bof3::image::Va kActor{0x904B34};          // byte (read as a dword by some): the acting actor
constexpr ::bof3::image::Va kAutoMode{0x904B35};       // byte
constexpr ::bof3::image::Va kBattleFlags{0x904AA8};    // dword; bit 4 of the low byte, bit 7 the critical hit
constexpr ::bof3::image::Va kPartyCount{0x904AB0};     // byte
// kPartyPick: now Battle_PartyStanding (symbols.toml). byte, tested against 1
constexpr ::bof3::image::Va kEnemyCount{0x904AB2};     // byte
// kEnemyPick: now Battle_EnemiesLeft (symbols.toml). byte, tested against 1; = kEnemyCount after set-up
constexpr ::bof3::image::Va kBoss{0x904AAA};           // byte: 0 an ordinary encounter, else the boss handler's index
constexpr ::bof3::image::Va kLayout{0x904AAC};         // byte (read as a dword): the battle layout
// kFormation: now Cond_Flags[0xd0] (symbols.toml). byte (read as a dword): the formation, 1..4
// kKindFlags: now Cond_Flags[0xd8] (symbols.toml). bitset by enemy kind
// kActionFlags: now Cond_Flags[0xf8] (symbols.toml). bitset by action id
// kHitBlock: now BattleSprites_HitBlock (symbols.toml). pointer; its byte +0xC bit 1
// kEnemyOffsets: now BattleE3_CurrentEnemy (symbols.toml). pointer; bytes +0xF2 / +0xF3
constexpr ::bof3::image::Va kActions{0x65C4D8};        // 0x18 bytes per action id: +0 flags, +1, +4 u16, +5
constexpr std::uint32_t kActionStride = 0x18;
constexpr ::bof3::image::Va kHitClass{0x657465};       // a byte every 0x1C, by the party record's +0x92
constexpr ::bof3::image::Va kHitSounds{0x64F10C};      // u16 sound ids, [class + critical * 7]
// kPartyOffsets: now BattleWin_MemberTargetOffsets (symbols.toml). two signed bytes per (sprite +8) + (Field_State +0x89) * 4
constexpr ::bof3::image::Va kNamePlaces{0x656A34};     // two u16 per layout: x, y
constexpr ::bof3::image::Va kBossHandlers{0x656954};   // a handler per boss index, tail-jumped to
constexpr ::bof3::image::Va kClutMap{0x7E06A0};        // 4 rows x 16 cells: the owner of each clut cell, 0xFF free
// kTints: now MoveScript_TintRecords (symbols.toml). MoveScript_TintRecords: 32 records of 12 bytes
constexpr ::bof3::image::Va kTintsEnd{0x7E0880};
inline auto& kCells = SharedScratch;  // SharedScratch: bytes 0 / 1 the clut map row / cell found
constexpr ::bof3::image::Va kCells_addr{SharedScratch_addr}; // kCells's address, for the raw accessors that take one
constexpr ::bof3::image::Va kClutCount{0x6528C4};      // bytes by the sprite's +0x28: entries / 16
constexpr ::bof3::image::Va kClutDivisor{0x6528CC};    // bytes by +0x28: slots per strip row
constexpr ::bof3::image::Va kClutStep{0x6528D4};       // bytes by +0x28: the column step
}  // namespace at

struct Callees {
    unsigned char (__cdecl* actor_out)(unsigned);             // 0x4456C0 (BB): 1 when the actor is absent or dead
    int (__cdecl* rand)();                                    // Rand 0x5B93D2
    void (__cdecl* mark_actor)(unsigned);                     // 0x446FB0 (BF): a bit per actor in the word 0x904B82
    unsigned char (__cdecl* inventory_put)(unsigned, unsigned, unsigned, unsigned);   // 0x590C90, nobody's
    unsigned char (__cdecl* item_class)(unsigned, unsigned);  // 0x591810 Item_UseFlags, ours since group TWO (2026-10-06)
    unsigned char (__cdecl* popup_slot)(unsigned, unsigned);  // 0x435180 (BB): a free 0x93A000 record
    short (__cdecl* add_clamped)(unsigned short*, unsigned);  // Stat_AddClamped
    unsigned char (__cdecl* add_cap100)(unsigned char*, unsigned);   // Stat_AddCap100
    unsigned char (__cdecl* window_open)(unsigned, unsigned); // 0x59E2D0, nobody's: claims window record n
    void (__cdecl* play_effect)(unsigned short);              // Sound_PlayEffect
    long (__cdecl* elevation)(long, long);                    // AreaMap_Elevation
    unsigned char (__cdecl* set_bank)(unsigned short);        // Sprite_SetAnimationBank
    void (__cdecl* update_screen)();                          // Sprite_UpdateScreen
    unsigned char (__cdecl* member_chance_b)(unsigned);       // 0x453910, Battle_MemberRollByAction (BE6)
    unsigned char (__cdecl* action_flag)(unsigned);           // 0x453A90, Battle_ActionBitSet (BE6)
    unsigned char (__cdecl* slots_full)(unsigned);            // 0x453AC0, Battle_MemberListFull (BE6)
    unsigned char (__cdecl* member_chance)(unsigned);         // 0x453A10 (ours)
    unsigned char (__cdecl* auto_allowed)(unsigned);          // 0x452DD0, Battle_AutoTargetCheck (R4A)
    void (__cdecl* auto_fixed)(unsigned);                     // 0x454290, Battle_MemberAutoFixed (R4A)
    void (__cdecl* no_target)();                              // 0x446B00, nobody's
    unsigned char (__cdecl* out_action)(unsigned);            // 0x454220 (ours)
    unsigned char (__cdecl* action_is_e)(unsigned);           // 0x454260, Battle_MemberActionIs0E (R4A)
    unsigned char (__cdecl* pick_party)(unsigned);            // 0x454310, Battle_RandomOtherMember (R4A)
    unsigned char (__cdecl* pick_enemy_a)(unsigned);          // Battle_RandomEnemy 0x435C80 (round twelve group BE2's)
    unsigned char (__cdecl* pick_enemy_b)(unsigned);          // 0x445730 (BE)
    unsigned char (__cdecl* clut_mark)(unsigned, unsigned, unsigned);   // 0x454DF0 (ours)
    unsigned char (__cdecl* clut_find)(unsigned);             // 0x454F30 (ours)
    unsigned char (__cdecl* clut_owner)(unsigned);            // 0x455140 (ours)
    void (__cdecl* boss_common)();                            // Battle_PlaceBossActors 0x494500 (R3G)
    void (__cdecl* boss_encounter)();                         // 0x4942A0 (ours)
    void (__cdecl* normal_encounter)();                       // 0x4942C0 (ours)
    void (__cdecl* setup_enemy)(unsigned, unsigned, long, long);   // 0x494320 (ours)
    void (__cdecl* copy_enemy)(unsigned, unsigned);           // 0x4946C0 (ours)
    void (__cdecl* enemy_offset)(unsigned, unsigned);         // 0x494F00 (ours)
    unsigned char (__cdecl* kind_flag)(unsigned);             // 0x494EA0 (ours)
};
extern const Callees kOriginals;
extern Callees g;

}  // namespace battle_sprites
