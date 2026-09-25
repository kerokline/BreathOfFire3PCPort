// Internal to field_hidden.cpp and field_hidden_fuzz.cpp: the addresses the
// fifteen functions touch that symbols.toml does not name, and every call
// they make - through pointers, so that the start-up fuzz can stand
// recording functions in for them, for the originals' copies and for ours
// alike. docs/field_hidden.md.
//
// Every pointer is typed at the width the originals pass - whole dwords -
// and every byte result as the whole eax, of which only al is meaningful:
// Capcom's callees leave the rest undefined, and so do the fuzz's recorders.
//
// Callees in no group of the eighth round (nobody's) go through the raw
// addresses below and are never bound here:
//   0x51C390  a party action's "blocked ahead" test: Sprite_ObjectAt two
//             steps ahead, or AreaMap_ByteAt 0xF2 there (u8, no arguments)
//   0x524870  an effect object of kind 0x34 at a cell (kind byte, x, z words)
//   0x5307C0  the zenny found: Sound_PlayEffect(0x106), the amount printed
//             into Text_Records, Msg_OpenSystem(5), Field_Request = 2,
//             0x591BE0(amount, 0)
//   0x5728D0  a cell's byte of the map cleared (x, z words)
//
// The dispatches that are not calls to a named function: two .data tables
// read in place, the index unchecked as the originals read them -
// PartyAction5_Form0States 0x65FBC0 (by byte +2) and PartyAction5_Forms
// 0x65FC18 (by word +0x2C). The fuzz swaps their entries.
#pragma once

#include <cstdint>

namespace field_hidden {

namespace at {

// .data
constexpr std::uint32_t kForm0States = 0x65FBC0;    // PartyAction5_Form0States: 3 entries by +2
constexpr std::uint32_t kForms = 0x65FC18;          // PartyAction5_Forms: 3 entries by u16 +0x2C
constexpr std::uint32_t kSlotChance = 0x669CB0;     // Encounter_SlotChance: u8 x 8
constexpr std::uint32_t kDirSteps = 0x6697B0;       // Field_DirectionSteps: two longs a direction, 8 rows, read unchecked
// ObjTrio's first member, +0x137 (the leader's); Member_EffectState waits while it is 8.
constexpr std::uint32_t kLeader137 = 0x802E77;
// The scratch bytes (DamageScratch, PSX 0x1F800000): byte 0 is AreaMap_Slope's
// "sloped" flag, left there by MapView_SlopeAt.
constexpr std::uint32_t kScratch = 0x903850;
// Sprite_Objects' / Sprite_ObjectsExtra's byte +0x80 (bit 0 "touched").
constexpr std::uint32_t kObjectFlag = 0x7DEF00;
constexpr std::uint32_t kExtraFlag = 0x802080;
constexpr unsigned kObjectStride = 0xA4;

// --- The encounter placement (inventory_ops_callees.h names the same) ------
constexpr std::uint32_t kScratchSprite = 0x6BDFF0;  // the sprite Encounter_Place makes Sprite_Current
constexpr std::uint32_t kFacing = 0x6BE070;         // u8: the formation's facing 0..3
constexpr std::uint32_t kPlaced = 0x6BE071;         // u8: enemies placed / reachable
constexpr std::uint32_t kEnemyCount = 0x6BE084;     // u8: Encounter_FillSlots' answer
constexpr std::uint32_t kRowUsed = 0x6BE088;        // unsigned char *: the row Encounter_PickRow chose
constexpr std::uint32_t kCentreX = 0x903780;        // i32 16.16: the fight's centre
constexpr std::uint32_t kCentreZ = 0x903784;
constexpr std::uint32_t kMemberPos = 0x7E06E0;      // i32 x, z per member
constexpr std::uint32_t kPartyList2 = 0x904065;     // u8 x 3: the members' ids the placement reads
constexpr std::uint32_t kBattleFacing = 0x904AAC;   // u8: kFacing, copied
constexpr std::uint32_t kEnemies = 0x939F00;        // 12 bytes x 8: +0 active, +1 kind, +4 x, +8 z
constexpr unsigned kEnemyStride = 12;
constexpr std::uint32_t kRows = 0x8C5580;           // Encounter_Rows: 8 rows of 9 bytes, +8 the weight
constexpr std::uint32_t kKinds = 0x8C55C8;          // 0x8C bytes per enemy kind: +0x86 size, +0x87 margin
constexpr unsigned kKindStride = 0x8C;

}  // namespace at

// Nobody's (see above).
constexpr std::uint32_t kBlockedAhead = 0x51C390;
constexpr std::uint32_t kSpawnAtCell = 0x524870;
constexpr std::uint32_t kFoundZenny = 0x5307C0;
constexpr std::uint32_t kClearCell = 0x5728D0;

using Handler = void (__cdecl*)();
using Byte0 = std::uint32_t (__cdecl*)();   // a u8 answer in al, no arguments

struct Callees {
    Byte0 script_tick;                                                   // Sprite_ScriptTick
    Byte0 script_tick_once;                                              // Sprite_ScriptTickOnce
    Byte0 effect_free;                                                   // Effect_FindFree
    void (__cdecl* set_animation)(unsigned);                             // Sprite_SetAnimation (a byte)
    std::uint32_t (__cdecl* ensure_animation)(unsigned);                 // Sprite_EnsureAnimation (a byte)
    Byte0 blocked_ahead;                                                 // 0x51C390, nobody's
    long (__cdecl* slope_at)(long, long, unsigned long);                 // MapView_SlopeAt
    long (__cdecl* ground_at)(long, long);                               // MapView_GroundAt
    void (__cdecl* play_effect)(unsigned);                               // Sound_PlayEffect (a word)
    std::uint32_t (__cdecl* object_at)(long, long, unsigned);            // Sprite_ObjectAt
    std::uint32_t (__cdecl* cell_pickup)(unsigned, unsigned);            // Field_CellPickup (ours)
    std::uint32_t (__cdecl* byte_at)(unsigned, unsigned);                // AreaMap_ByteAt (two words)
    void (__cdecl* spawn_at_cell)(unsigned, unsigned, unsigned);         // 0x524870, nobody's
    int (__cdecl* rand)();                                               // Rand
    void (__cdecl* found_zenny)(unsigned);                               // 0x5307C0, nobody's
    unsigned char* (__cdecl* item_name)(unsigned, unsigned);             // Item_NamePtr
    std::uint32_t (__cdecl* inventory_add)(unsigned, unsigned, unsigned);  // Inventory_Add
    void (__cdecl* msg_open)(unsigned);                                  // Msg_OpenSystem
    void (__cdecl* clear_cell)(unsigned, unsigned);                      // 0x5728D0, nobody's
    unsigned char* (__cdecl* frame_upload)(unsigned);                    // Sprite_SetFrameQueueUpload
    void (__cdecl* script_start)(unsigned);                              // Sprite_ScriptStart (a word)
    void (__cdecl* copy_frames)(unsigned, unsigned char*, unsigned);     // Sprite_CopyFrames (ours)
    long (__cdecl* elevation)(long, long);                               // AreaMap_Elevation
    Byte0 pick_row;                                                      // Encounter_PickRow (ours)
    std::uint32_t (__cdecl* fill_slots)(const unsigned char*);           // Encounter_FillSlots (ours)
    Byte0 place_party;                                                   // Encounter_PlaceParty
    Byte0 place_enemies;                                                 // Encounter_PlaceEnemies
    Byte0 reachable;                                                     // Encounter_EnemiesReachable
    void (__cdecl* push_matrix)();                                       // Gte_PushMatrix
    void (__cdecl* pop_matrix)();                                        // Gte_PopMatrix
    void (__cdecl* aim_camera)(unsigned);                                // Encounter_AimCamera (a byte)
    std::uint32_t (__cdecl* party_count)(unsigned);                      // Party_Count
    std::uint32_t (__cdecl* on_screen)(long, long, unsigned, unsigned);  // Encounter_OnScreen (two bytes)
};

extern const Callees kOriginals;
extern Callees g;

// The start-up fuzz (field_hidden_fuzz.cpp): clones every function of this
// file with each call out re-aimed at a recording stand-in and the two .data
// tables' entries swapped, runs ours against the clones from the same random
// state, and ends the process through bof3::Fatal on any difference.
void SelfTest();

}  // namespace field_hidden
