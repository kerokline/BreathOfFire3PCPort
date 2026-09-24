// Internal to inventory_ops.cpp and inventory_ops_fuzz.cpp: the addresses the
// encounter placement and the battle intro's party steps touch that have no
// name in symbols.toml, and every call the 25 functions make - through
// pointers, so that the start-up fuzz can stand recording functions in for
// them, for the originals' copies and for ours alike. Most callees are ours
// in this module; through the pointers each function is still tested alone.
// docs/inventory_ops.md.
#pragma once

#include <cstdint>

namespace inventory_ops {

namespace at {

// --- The placement's state (PSX 0x801CE04C..0x801CE0E0) -------------------
constexpr std::uint32_t kFacing = 0x6BE070;       // u8: the formation's facing 0..3 (0x591F30 sets it)
constexpr std::uint32_t kPlaced = 0x6BE071;       // u8: enemies placed / reachable, counted in memory
constexpr std::uint32_t kOrder = 0x6BE074;        // u8 x 3: the members in placing order
constexpr std::uint32_t kMembers = 0x6BE078;      // 4 bytes per member: +0 placed, +1 wide (its record is 2 or 6)
constexpr std::uint32_t kEnemyCount = 0x6BE084;   // u8: the enemies 0x5925A0 chose
constexpr std::uint32_t kScratchSprite = 0x6BDFF0;   // the sprite 0x591F30 makes Sprite_Current (+0x3C written)
constexpr std::uint32_t kCentreX = 0x903780;      // i32 16.16: the fight's centre (PSX 0x80143F24)
constexpr std::uint32_t kCentreZ = 0x903784;
constexpr std::uint32_t kMemberPos = 0x7E06E0;    // i32 x, z per member (PSX 0x80143F2C)
constexpr std::uint32_t kHeightAvg = 0x939860;    // i32: the mean ground height, low 5 bits cleared
constexpr std::uint32_t kEnemies = 0x939F00;      // 12 bytes x 8 (PSX 0x801463D8): +0 active, +1 kind, +4 x, +8 z
constexpr unsigned kEnemyStride = 12;
constexpr std::uint32_t kKinds = 0x8C55C8;        // 0x8C bytes per enemy kind: +0x86 its size, +0x87 its margin
constexpr unsigned kKindStride = 0x8C;
constexpr std::uint32_t kCellMap = 0x8C3D80;      // a nibble per cell, AreaMap_Header's bytes 0 / 1 the extent

// --- Read-only tables of the placement (.data) ------------------------------
constexpr std::uint32_t kPartyOffsets = 0x6698B0;   // i32 x, z; by ((last^2 + formation) * 3 + member)
constexpr std::uint32_t kEnemyOffsets = 0x669B60;   // i32 x, z; by the enemy count's triangle + placed
constexpr std::uint32_t kJitter = 0x669AF8;         // s8 x, z pairs, 49 of them, in half cells
constexpr std::uint32_t kRecentre = 0x669C80;       // i32 x, z, 6 of them: the centre's retries
constexpr std::uint32_t kCameraOffsets = 0x669CB8;  // s16 x, z per facing

// --- The party and the battle intro -----------------------------------------
constexpr std::uint32_t kFormation = 0x904060;    // u8: the party's formation (Encounter_PlaceParty's first argument)
constexpr std::uint32_t kPartyList = 0x904062;    // u8 x 3: the party (event_ops' list)
constexpr std::uint32_t kPartyList2 = 0x904065;   // u8 x 3: the second list the placement reads
constexpr std::uint32_t kMemberRecord = 0x66972C; // u8 per member id: its actor record (symbols.toml's MoveScript_EffectState)
constexpr std::uint32_t kRecords = 0x903A70;      // the actor records, 0xA4 each (Field_ActorStates is +0x10)
constexpr unsigned kRecordSize = 0xA4;
constexpr std::uint32_t kBattleFacing = 0x904AAC; // u8: kFacing, copied by 0x591F30
constexpr std::uint32_t kInitiative = 0x904AE4;   // u8: Encounter_RollInitiative's 0 / 1 / 2
constexpr std::uint32_t kBattleFlags = 0x904AE5;  // u8: bit 2 skips the shade steps, bit 4 the script step
constexpr std::uint32_t kFaceTable = 0x660B3C;    // u8 per facing: the direction the party turns to
constexpr std::uint32_t kSideSteps = 0x660B1C;    // s8 x, z per facing, in cells
constexpr std::uint32_t kAlertOffsets = 0x660B24; // u8 x, y per member id: the alert effect's offset
constexpr std::uint32_t kShadeSource = 0x80D380;  // Gfx_ClutStripSource row 15: 64 bytes per sprite +5
constexpr std::uint32_t kShadeSaved = 0x80D440;   // the copy three CLUTs on, restored after the fade
constexpr std::uint32_t kShadeClut = 0x811380;    // Gfx_ClutStrip row 15 (event_objs' kShadeClut)
constexpr std::uint32_t kAnimBuffer = 0x8C5D80;   // the buffer 0x5891C0 is given, 0xA00 bytes
constexpr unsigned kObjStride = 0x14C;            // ObjTrio's stride

}  // namespace at

// Group BI's calls into the next round's functions go through this address,
// never a name (the round's rule): 0x5891C0 (anim, 0, buffer, size) - calls
// 0x589160 (which copies from the sprite's +0x50 into the buffer), sets the
// sprite's byte +0x4B to the animation and calls 0x589350.
constexpr std::uint32_t kSetAnimFrom = 0x5891C0;

struct Callees {
    int (__cdecl* party_count)(unsigned);                                         // Party_Count (field_event.cpp)
    long (__cdecl* elevation)(long, long);                                        // AreaMap_Elevation (map_cells.cpp)
    unsigned char (__cdecl* object_at)(long, long, unsigned);                     // Sprite_ObjectAt (field_blocked.cpp)
    long (__cdecl* rot_trans_pers)(const short*, unsigned long*, long*);          // Gte_RotTransPers
    short* (__cdecl* rot_matrix)(const short*, short*);                           // Gte_RotMatrix
    void (__cdecl* apply_matrix)(const short*, const short*, long*);              // Gte_ApplyMatrix
    void (__cdecl* set_rot_matrix)(const unsigned long*);                         // Gte_SetRotMatrix
    void (__cdecl* set_trans_matrix)(const unsigned long*);                       // Gte_SetTransMatrix
    int (__cdecl* rand)();                                                        // Rand (the CRT's)
    long (__cdecl* ground_at)(long, long);                                        // MapView_GroundAt
    unsigned char (__cdecl* effect_free)();                                       // Effect_FindFree
    void (__cdecl* set_anim_from)(unsigned, unsigned, unsigned char*, unsigned);  // 0x5891C0 (next round)
    unsigned char (__cdecl* script_tick)();                                       // Sprite_ScriptTick
    void (__cdecl* shade_begin)();                                                // Sprite_ShadeFadeBegin
    unsigned char (__cdecl* shade_raise)(unsigned);                               // Sprite_ShadeRaise
    unsigned char (__cdecl* script_once)();                                       // Sprite_ScriptTickOnce
    void (__cdecl* set_animation)(unsigned char);                                 // Sprite_SetAnimation
    // ours, in this file
    unsigned char (__cdecl* place_member)(unsigned, unsigned, unsigned, unsigned);   // Encounter_PlaceMember
    unsigned char (__cdecl* member_clear)(unsigned);                                 // Encounter_MemberClear
    unsigned char (__cdecl* member_stands)(unsigned);                                // Encounter_MemberStands
    unsigned char (__cdecl* enemy_clear)(unsigned, long, long, unsigned);            // Encounter_EnemyClear
    unsigned char (__cdecl* cell_nibble)(unsigned, unsigned);                        // AreaMap_CellNibble
    long (__cdecl* project)(long, long, long, unsigned long*);                       // Encounter_Project
    unsigned char (__cdecl* apart)(long, long, long, long, unsigned, unsigned);      // Encounter_Apart
    unsigned char (__cdecl* cell_fits)(long, long, unsigned);                        // Encounter_CellFits
    unsigned char (__cdecl* path_clear)(unsigned, unsigned, unsigned, unsigned);     // Encounter_PathClear
    unsigned (__cdecl* short_abs)(unsigned);                                         // Short_Abs
    short (__cdecl* short_sign)(unsigned);                                           // Short_Sign
    unsigned char (__cdecl* step_open)(unsigned, unsigned, unsigned, unsigned);      // Encounter_StepOpen
    unsigned char (__cdecl* turn_sense)(unsigned);                                   // Sprite_TurnSense
    unsigned char (__cdecl* shade_lower)(unsigned);                                  // Sprite_ShadeLower
};

extern const Callees kOriginals;
extern Callees g;

// BOF3X_SHADOW=inventory_ops: the start-up fuzz, inventory_ops_fuzz.cpp.
// Clones every original before InventoryOps_Inject patches it.
void SelfTest();

}  // namespace inventory_ops
