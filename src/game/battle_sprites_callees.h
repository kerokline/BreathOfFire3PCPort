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

namespace battle_sprites {

// The addresses the group reads or writes that symbols.toml does not name
// (docs/battle_sprites.md, section 2).
namespace at {
constexpr std::uint32_t kParty = 0x802D40;          // ObjTrio: three 0x14C-byte party records, actor 0..2
constexpr std::uint32_t kPartyStride = 0x14C;
constexpr std::uint32_t kEnemies = 0x93B960;        // eight 0x128-byte enemy records, actor 3..10 (index actor - 3)
constexpr std::uint32_t kEnemyStride = 0x128;
constexpr std::uint32_t kEnemyData = 0x8C55C8;      // the loaded enemy data, 0x8C bytes per id: +0 the 12-byte name
constexpr std::uint32_t kEnemyDataStride = 0x8C;
constexpr std::uint32_t kEncounter = 0x939F00;      // eight 12-byte records: +0 used, +1 id, +4 x, +8 z
constexpr std::uint32_t kPopups = 0x93A000;         // 0x84-byte records (0x435180 hands out a free one)
constexpr std::uint32_t kPopupStride = 0x84;
constexpr std::uint32_t kWindows = 0x803160;        // 0x24-byte window records (0x59E2D0 claims one)
constexpr std::uint32_t kWindowStride = 0x24;
constexpr std::uint32_t kAction = 0x904B40;         // pointer: the current action; its u16 at +2 is the action id
constexpr std::uint32_t kTarget = 0x904B44;         // byte: the target actor, or 0x40 / 0x80 / 0xC0 a side
constexpr std::uint32_t kActor = 0x904B34;          // byte (read as a dword by some): the acting actor
constexpr std::uint32_t kAutoMode = 0x904B35;       // byte
constexpr std::uint32_t kBattleFlags = 0x904AA8;    // dword; bit 4 of the low byte, bit 7 the critical hit
constexpr std::uint32_t kPartyCount = 0x904AB0;     // byte
constexpr std::uint32_t kPartyPick = 0x904AB1;      // byte, tested against 1
constexpr std::uint32_t kEnemyCount = 0x904AB2;     // byte
constexpr std::uint32_t kEnemyPick = 0x904AB3;      // byte, tested against 1; = kEnemyCount after set-up
constexpr std::uint32_t kBoss = 0x904AAA;           // byte: 0 an ordinary encounter, else the boss handler's index
constexpr std::uint32_t kLayout = 0x904AAC;         // byte (read as a dword): the battle layout
constexpr std::uint32_t kFormation = 0x904060;      // byte (read as a dword): the formation, 1..4
constexpr std::uint32_t kKindFlags = 0x904068;      // bitset by enemy kind
constexpr std::uint32_t kActionFlags = 0x904088;    // bitset by action id
constexpr std::uint32_t kHitBlock = 0x904B50;       // pointer; its byte +0xC bit 1
constexpr std::uint32_t kEnemyOffsets = 0x939AD8;   // pointer; bytes +0xF2 / +0xF3
constexpr std::uint32_t kActions = 0x65C4D8;        // 0x18 bytes per action id: +0 flags, +1, +4 u16, +5
constexpr std::uint32_t kActionStride = 0x18;
constexpr std::uint32_t kHitClass = 0x657465;       // a byte every 0x1C, by the party record's +0x92
constexpr std::uint32_t kHitSounds = 0x64F10C;      // u16 sound ids, [class + critical * 7]
constexpr std::uint32_t kPartyOffsets = 0x64DF70;   // two signed bytes per (sprite +8) + (Field_State +0x89) * 4
constexpr std::uint32_t kNamePlaces = 0x656A34;     // two u16 per layout: x, y
constexpr std::uint32_t kBossHandlers = 0x656954;   // a handler per boss index, tail-jumped to
constexpr std::uint32_t kClutMap = 0x7E06A0;        // 4 rows x 16 cells: the owner of each clut cell, 0xFF free
constexpr std::uint32_t kTints = 0x7E0700;          // MoveScript_TintRecords: 32 records of 12 bytes
constexpr std::uint32_t kTintsEnd = 0x7E0880;
constexpr std::uint32_t kCells = 0x903850;          // DamageScratch: bytes 0 / 1 the clut map row / cell found
constexpr std::uint32_t kClutCount = 0x6528C4;      // bytes by the sprite's +0x28: entries / 16
constexpr std::uint32_t kClutDivisor = 0x6528CC;    // bytes by +0x28: slots per strip row
constexpr std::uint32_t kClutStep = 0x6528D4;       // bytes by +0x28: the column step
}  // namespace at

struct Callees {
    unsigned char (__cdecl* actor_out)(unsigned);             // 0x4456C0 (BB): 1 when the actor is absent or dead
    int (__cdecl* rand)();                                    // Rand 0x5B93D2
    void (__cdecl* mark_actor)(unsigned);                     // 0x446FB0 (BF): a bit per actor in the word 0x904B82
    unsigned char (__cdecl* inventory_put)(unsigned, unsigned, unsigned, unsigned);   // 0x590C90, nobody's
    unsigned char (__cdecl* item_class)(unsigned, unsigned);  // 0x591810, nobody's
    unsigned char (__cdecl* popup_slot)(unsigned, unsigned);  // 0x435180 (BB): a free 0x93A000 record
    short (__cdecl* add_clamped)(unsigned short*, unsigned);  // Stat_AddClamped
    unsigned char (__cdecl* add_cap100)(unsigned char*, unsigned);   // Stat_AddCap100
    unsigned char (__cdecl* window_open)(unsigned, unsigned); // 0x59E2D0, nobody's: claims window record n
    void (__cdecl* play_effect)(unsigned short);              // Sound_PlayEffect
    long (__cdecl* elevation)(long, long);                    // AreaMap_Elevation
    unsigned char (__cdecl* set_bank)(unsigned short);        // Sprite_SetAnimationBank
    void (__cdecl* update_screen)();                          // Sprite_UpdateScreen
    unsigned char (__cdecl* member_chance_b)(unsigned);       // 0x453910, nobody's
    unsigned char (__cdecl* action_flag)(unsigned);           // 0x453A90, nobody's
    unsigned char (__cdecl* slots_full)(unsigned);            // 0x453AC0, nobody's
    unsigned char (__cdecl* member_chance)(unsigned);         // 0x453A10 (ours)
    unsigned char (__cdecl* auto_allowed)(unsigned);          // 0x452DD0, nobody's
    void (__cdecl* auto_fixed)(unsigned);                     // 0x454290, nobody's
    void (__cdecl* no_target)();                              // 0x446B00, nobody's
    unsigned char (__cdecl* out_action)(unsigned);            // 0x454220 (ours)
    unsigned char (__cdecl* action_is_e)(unsigned);           // 0x454260, nobody's
    unsigned char (__cdecl* pick_party)(unsigned);            // 0x454310, nobody's
    unsigned char (__cdecl* pick_enemy_a)(unsigned);          // 0x435C80, nobody's
    unsigned char (__cdecl* pick_enemy_b)(unsigned);          // 0x445730 (BE)
    unsigned char (__cdecl* clut_mark)(unsigned, unsigned, unsigned);   // 0x454DF0 (ours)
    unsigned char (__cdecl* clut_find)(unsigned);             // 0x454F30 (ours)
    unsigned char (__cdecl* clut_owner)(unsigned);            // 0x455140 (ours)
    void (__cdecl* boss_common)();                            // 0x494500, nobody's
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
