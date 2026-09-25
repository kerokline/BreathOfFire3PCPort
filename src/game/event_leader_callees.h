// Internal to event_leader.cpp and event_leader_fuzz.cpp: the addresses the
// leader's states 3, 4, 5, 7 and 10, its sub-state 2 and the step's landing
// touch that symbols.toml does not name, and every call they make - through
// pointers, so that the start-up fuzz can stand recording functions in for
// them, for the originals' copies and for ours alike. Calls into functions
// nobody owns go through raw addresses here (docs/takeover-queue-round8.md,
// "The rule for calls across groups"). docs/event_leader.md.
//
// Five dispatches are not calls to a named function: each reads a .data table
// by a byte, unchecked, as the original does - ours reads the same memory
// (tables below), and the fuzz swaps the tables' entries for recorders.
#pragma once

#include <cstdint>

namespace event_leader {

namespace at {

// The .data dispatch tables (symbols.toml [[data]], this group's).
constexpr std::uint32_t kSwapSteps = 0x660968;       // 5 handlers by Sprite_Current +2 (state 3)
constexpr std::uint32_t kMenuSteps = 0x66097C;       // 2 (state 4)
constexpr std::uint32_t kEncounterSteps = 0x660984;  // 2 (state 5)
constexpr std::uint32_t kPassageSteps = 0x66098C;    // 4 (state 7)
constexpr std::uint32_t kActionBySet = 0x6609D0;     // 19 by the party set 0x90412C & 0x7F (state 10)
constexpr unsigned kSwapCount = 5, kMenuCount = 2, kEncounterCount = 2, kPassageStepCount = 4, kActionCount = 19;
// Constant tables.
constexpr std::uint32_t kFormation = 0x6608F6;       // s8 (dx, dz) pairs by (i + 2 * direction), i = 1, 2
constexpr std::uint32_t kEncounterAreas = 0x660A90;  // 10 (area, encounter area) word pairs, to 0x660AB8
constexpr std::uint32_t kEncounterAreasEnd = 0x660AB8;
constexpr std::uint32_t kEventCells = 0x660B10;      // cell codes with an event, 0xFF-ended
constexpr std::uint32_t kBlockingCells = 0x660C24;   // cell codes that block, 0xFF-ended
// The party (field-event.md section 2).
constexpr std::uint32_t kMemberStride = 0x14C;
constexpr std::uint32_t kLeaderX = 0x802D74;         // ObjTrio +0x34 / +0x38 (dwords), +0x3E (word)
constexpr std::uint32_t kLeaderZ = 0x802D78;
constexpr std::uint32_t kLeaderHeight = 0x802D7E;
constexpr std::uint32_t kActorRecords = 0x903A70;    // stride 0xA4
constexpr std::uint32_t kActorStride = 0xA4;
constexpr std::uint32_t kPartyList = 0x904062;       // the first 3-byte party list
constexpr std::uint32_t kPalettes = 0x80D380;        // 64 bytes per party slot (field-event.md)
// The step's scratch (event_ops_callees.h's terms).
constexpr std::uint32_t kScratch = 0x903850;         // u8: MapView_SlopeAt's "a slope" byte
constexpr std::uint32_t kScratch2 = 0x903852;        // u16: the found spot's ground
constexpr std::uint32_t kTargetX = 0x903858;         // s32 each: the found spot
constexpr std::uint32_t kTargetZ = 0x90385C;
// Field_ChangeArea's arguments as the step's exits leave them (event_ops).
constexpr std::uint32_t kExitArea = 0x937F82;        // u16
constexpr std::uint32_t kExitX = 0x903860;           // s32
constexpr std::uint32_t kExitZ = 0x90384C;           // s32
constexpr std::uint32_t kExitKind = 0x905B88;        // u8
constexpr std::uint32_t kReturnPoint = 0x904148;     // x, z dwords, the area word at +8, a byte at +0xA
constexpr std::uint32_t kEdgeArea = 0x802290;        // u16: the area the map's edge leads to
constexpr std::uint32_t kEdgeX = 0x7E091C;           // s32 each
constexpr std::uint32_t kEdgeZ = 0x7E0920;
constexpr std::uint32_t kLandByte = 0x904EE0;        // u8, 0 on the 0xAF exit
constexpr std::uint32_t kLandMode = 0x937F98;        // u8, 0xC on the 0xAF exit
constexpr std::uint32_t kButtonConfirm = 0x903580;   // u16 (input-script.md section 4)
// The pending walk-in (Area_Enter's 0x904EF0.., symbols.toml 0x533710).
constexpr std::uint32_t kPendingFlag = 0x904EF0;     // u8
constexpr std::uint32_t kPendingCount = 0x904EF2;    // u8: members walked in
constexpr std::uint32_t kPendingX = 0x904EF4;        // s32 each; +2 the whole parts
constexpr std::uint32_t kPendingZ = 0x904EF8;
// The passage's content.
constexpr std::uint32_t kPassageFlags = 0x90410C;    // bits by the entry's +4 (Flags_Test / Flags_Set)
constexpr std::uint32_t kPassageCount = 0x90413C;    // s32, one more per content taken
constexpr std::uint32_t kPartySet = 0x90412C;        // u8, & 0x7F the row (field-event.md)
constexpr std::uint32_t kEventBattle = 0x904AAA;     // u8 (battle_flow_callees.h)
// Field_ScriptFlags' high byte and Field_ScriptFlags2's two, tested and set by
// byte as the original does.
constexpr std::uint32_t kFlagsHi = 0x9039A3;
constexpr std::uint32_t kFlags2Lo = 0x905BA4;
constexpr std::uint32_t kFlags2Hi = 0x905BA5;

}  // namespace at

// Functions nobody owns - raw addresses (docs/event_leader.md section 4).
namespace fn {
constexpr std::uint32_t kPathClear = 0x52EC20;       // u8(x, z, raised): the way from the leader to (x, z) (unread whole)
constexpr std::uint32_t kGiveZenny = 0x5307C0;       // void(n): sound 0x106, "%d" of n, Msg_OpenSystem(5), request 2, 0x591BE0(n, 0)
constexpr std::uint32_t kAreaExits = 0x462AC0;       // const u8*(): the area's 4-byte exit records (x, z, area, kind)
constexpr std::uint32_t kWayBlockedWide = 0x535830;  // u8(x, z, ground): Field_WayBlocked for a raised sprite
}  // namespace fn

using Handler = void (__cdecl*)();

struct Callees {
    // this file's, each through here so each is tested alone
    void (__cdecl* step_lands)();                                       // Field_LeaderStepLands
    unsigned char (__cdecl* spot_free)(long, long, unsigned, unsigned); // Field_SpotFree
    unsigned char (__cdecl* way_blocked4)(long, long, long);            // Field_WayBlocked4
    unsigned char (__cdecl* cells_block)(long, long, unsigned);         // Field_CellsBlock
    unsigned char (__cdecl* way_blocked)(long, long, unsigned, long);   // Field_WayBlocked
    // other files' functions
    unsigned char (__cdecl* step_tick)();                               // Field_LeaderStepTick
    void (__cdecl* equip_tick)();                                       // Field_EquipTick
    void (__cdecl* bit20_tick)();                                       // Field_Bit20Tick
    void (__cdecl* floor_damage)();                                     // Field_FloorDamage
    unsigned char (__cdecl* tile89)(unsigned);                          // Field_Tile89
    unsigned char (__cdecl* tile8a)(unsigned);                          // Field_Tile8A
    unsigned char (__cdecl* tile_d0)();                                 // Field_TileD0
    unsigned char (__cdecl* tile_a4)();                                 // Field_TileA4
    unsigned char (__cdecl* test_fb)(short, short);                     // MoveCmd_TestFB
    unsigned char (__cdecl* byte_at)(short, short);                     // AreaMap_ByteAt
    unsigned char (__cdecl* leader_animation)(unsigned);                // Field_LeaderAnimation
    void (__cdecl* change_area)(unsigned, int, int, unsigned);          // Field_ChangeArea
    unsigned char (__cdecl* link_at)(unsigned, unsigned);               // Area_LinkAt
    unsigned char (__cdecl* cells_all)(long, long, unsigned, unsigned, unsigned);   // AreaMap_CellsAll
    unsigned char (__cdecl* cells_none)(long, long, unsigned, unsigned, unsigned);  // AreaMap_CellsNone
    unsigned char (__cdecl* ensure_animation)(unsigned char);           // Sprite_EnsureAnimation
    void (__cdecl* jump_start)();                                       // Field_JumpStart
    void (__cdecl* clear_steps)();                                      // Sprite_ClearSteps
    unsigned char (__cdecl* encounter_due)();                           // Field_EncounterDue
    unsigned char (__cdecl* effect_test)(unsigned);                     // Field_LeaderEffectTest
    int (__cdecl* arrive_hook)(long, long);                             // Scenario_ArriveHook
    void (__cdecl* bit80_tick)();                                       // Field_Bit80Tick
    void (__cdecl* leader_walk)();                                      // Field_LeaderWalk
    void (__cdecl* clear_state)(unsigned);                              // Member_ClearState
    void (__cdecl* shade_begin)();                                      // Sprite_ShadeFadeBegin
    long (__cdecl* ground_at)(long, long);                              // MapView_GroundAt
    void (__cdecl* set_animation)(unsigned char);                       // Sprite_SetAnimation
    void (__cdecl* swap_fields)(unsigned, unsigned, unsigned);          // ObjTrio_SwapFields
    void (__cdecl* member_sprite)(unsigned, unsigned);                  // Field_MemberSprite
    void (__cdecl* load_palette)(unsigned short*, unsigned);            // Sprite_LoadPalette
    void (__cdecl* set_elevation)(int);                                 // MapView_SetElevation
    void (__cdecl* roll_initiative)();                                  // Encounter_RollInitiative
    unsigned char (__cdecl* script_tick)();                             // Sprite_ScriptTick
    const unsigned char* (__cdecl* passage_ahead)();                    // Area_PassageAhead
    void (__cdecl* script_run)(const unsigned char*);                   // EventScript_Run
    void (__cdecl* open_script)(unsigned short);                        // Msg_OpenScript
    void (__cdecl* open_system)(unsigned);                              // Msg_OpenSystem
    unsigned char (__cdecl* flags_test)(const unsigned char*, unsigned);   // Flags_Test
    void (__cdecl* flags_set)(unsigned char*, unsigned);                // Flags_Set
    void (__cdecl* play_effect)(unsigned short);                        // Sound_PlayEffect
    unsigned char* (__cdecl* item_name)(unsigned, unsigned);            // Item_NamePtr
    unsigned char (__cdecl* inventory_add)(unsigned, unsigned, unsigned);  // Inventory_Add (the original pushes a fourth 0, unread)
    unsigned char (__cdecl* test_fc)(short, short);                     // MoveCmd_TestFC
    long (__cdecl* slope_at)(long, long, unsigned long);                // MapView_SlopeAt
    unsigned char (__cdecl* object_at)(long, long, unsigned);           // Sprite_ObjectAt
    // raw addresses (namespace fn)
    unsigned char (__cdecl* path_clear)(long, long, unsigned);
    void (__cdecl* give_zenny)(int);
    const unsigned char* (__cdecl* area_exits)();
    unsigned char (__cdecl* way_blocked_wide)(long, long, long);
};

extern const Callees kOriginals;
extern Callees g;

// BOF3X_SHADOW=event_leader: the start-up fuzz, event_leader_fuzz.cpp. Clones
// every original before EventLeader_Inject patches it.
void SelfTest();

}  // namespace event_leader
