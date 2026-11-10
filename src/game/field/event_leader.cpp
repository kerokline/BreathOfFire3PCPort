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

using move_script::SetLong;

namespace {
template <typename T> T Fn(std::uint32_t address) { return reinterpret_cast<T>(static_cast<std::uintptr_t>(address)); }
}  // namespace


}  // namespace event_leader

using namespace event_leader;

// The record views (game/record_views.h): rv::Current<T>() reads
// Sprite_Current afresh at each use, as the original does.
// rv::As<records::SpriteObject>(p) views a
// sprite-family pointer; it is used below +0x80 only (the sprite head every
// family shares) unless the pointer is a Sprite_Objects / Sprite_ObjectsExtra /
// Sprite_Kind2 record.

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
}
