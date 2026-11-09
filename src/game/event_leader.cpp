// The event script's leader, steps and placements (group DC of the eighth
// round) - docs/event_leader.md for each function, its evidence, the tables
// and the fuzz.
//
//   - The leader's sub-state 2 (Field_LeaderStepping, table 0x660954 entry 2)
//     and where a step lands (Field_LeaderStepLands), round six's leftovers.
//   - Five of the leader's states (the table 0x660918): 3 the party swap, 4
//     the menu, 5 the encounter, 7 a passage's content, 10 the action by
//     party set - the three dispatchers and their sub-states.
//   - The spot a member can stand on (Field_SpotFree) and the way-blocked
//     tests under it (Field_WayBlocked, Field_WayBlocked4, Field_CellsBlock).
//   - The encounter's area, the event cells and the cell's exit (0x5317F0,
//     0x531920, 0x531AF0), and the pending jump's walk-in (0x533780).
//
// Terms as event_ops.cpp's: "SC" is Sprite_Current and "FS" Field_State, read
// afresh at every use as the original reads [0x937F88] / [0x905D98] (a callee
// may move either). Every call goes through event_leader::g
// (event_leader_callees.h), so that the start-up fuzz can stand recorders in.
#include "game/event_leader.h"

#include <cstdint>
#include <cstdlib>
#include <cstring>

#include "bof3/symbols.gen.h"
#include "game/event_leader_callees.h"
#include "game/move_script_bytes.h"
#include "game/record_views.h"
#include "game/records.h"
#include "hook/detour.h"
#include "hook/log.h"

namespace event_leader {

using move_script::At;
using move_script::SetLong;
using move_script::Word;

namespace {
template <typename T> T Fn(std::uint32_t address) { return reinterpret_cast<T>(static_cast<std::uintptr_t>(address)); }
}  // namespace

const Callees kOriginals = {
    Field_LeaderStepLands, Field_SpotFree, Field_WayBlocked4, Field_CellsBlock, Field_WayBlocked,
    Field_LeaderStepTick, Field_EquipTick, Field_Bit20Tick, Field_FloorDamage, Field_Tile89, Field_Tile8A,
    Field_TileD0, Field_TileA4, MoveCmd_TestFB, AreaMap_ByteAt, Field_LeaderAnimation, Field_ChangeArea,
    Area_LinkAt, AreaMap_CellsAll, AreaMap_CellsNone, Sprite_EnsureAnimation, Field_JumpStart, Sprite_ClearSteps,
    Field_EncounterDue, Field_LeaderEffectTest, Scenario_ArriveHook, Field_Bit80Tick, Field_LeaderWalk,
    Member_ClearState, Sprite_ShadeFadeBegin, MapView_GroundAt, Sprite_SetAnimation, ObjTrio_SwapFields,
    Field_MemberSprite, Sprite_LoadPalette, MapView_SetElevation, Encounter_RollInitiative, Sprite_ScriptTick,
    Area_PassageAhead, EventScript_Run, Msg_OpenScript, Msg_OpenSystem, Flags_Test, Flags_Set, Sound_PlayEffect,
    Item_NamePtr, Inventory_Add, MoveCmd_TestFC, MapView_SlopeAt, Sprite_ObjectAt,
    Fn<unsigned char (__cdecl*)(long, long, unsigned)>(::bof3::Code(fn::kPathClear.v)),
    Fn<void (__cdecl*)(int)>(::bof3::Code(fn::kGiveZenny.v)),
    Fn<const unsigned char* (__cdecl*)()>(::bof3::Code(fn::kAreaExits.v)),
    Fn<unsigned char (__cdecl*)(long, long, long)>(::bof3::Code(fn::kWayBlockedWide.v)),
};
Callees g = kOriginals;

// DIV-0121: 1 through PatchBytes("EdgeNoArea") in EventLeader_Inject, after
// the fuzz, which runs Capcom's rule.
std::uint32_t g_edge_no_area = 0;

}  // namespace event_leader

using namespace event_leader;

// The record views (game/record_views.h): rv::Current<T>() reads
// Sprite_Current afresh at each use, as the original does.
// rv::As<records::SpriteObject>(p) views a
// sprite-family pointer; it is used below +0x80 only (the sprite head every
// family shares) unless the pointer is a Sprite_Objects / Sprite_ObjectsExtra /
// Sprite_Kind2 record.

namespace {

unsigned char* Member(unsigned i) { return rv::Bytes(&ObjTrio[i]); }

// `cdq / xor / sub`: the absolute value with the original's wrap (the most
// negative dword stays negative).
std::int32_t Abs(std::uint32_t v) {
    const std::uint32_t s = static_cast<std::uint32_t>(static_cast<std::int32_t>(v) >> 31);
    return static_cast<std::int32_t>((v ^ s) - s);
}

}  // namespace

// --- the leader's step ------------------------------------------------------------

// --- state 3: the party swap -----------------------------------------------------

// --- state 4: the menu --------------------------------------------------------

// --- state 5: the encounter ----------------------------------------------------

// --- state 7: a passage's content ------------------------------------------------

// --- state 10: the action by party set ------------------------------------------

// --- the cells and the exits -----------------------------------------------------

// --- the way blocked -------------------------------------------------------------

void EventLeader_Inject() {
    if (bof3::WantsShadow("event_leader")) event_leader::SelfTest();
    {
        // DIV-0121: armed after the fuzz above, which sees Capcom's rule.
        const std::uint8_t was[] = {0, 0, 0, 0}, is[] = {1, 0, 0, 0};
        bof3::PatchBytes("EdgeNoArea", Low32(&event_leader::g_edge_no_area),
                         was, is, 4);
        bof3::Log("DIV-0121    a map edge that leads to no area (a game loaded in an encounter field) holds (on unless the line above says OFF)");
    }
}
