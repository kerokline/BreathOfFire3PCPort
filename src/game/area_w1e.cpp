// World 1's areas 65 and 67: the code of the PSX's BIN/WORLD01/AREA065 and
// AREA067.EMI compiled into the exe at 0x40B8C0..0x40CEEC - 49 functions,
// each read to its last instruction with capstone (2026-09-28) and taken
// through the area harness (area_harness.h). Round ten group AR1E;
// docs/area_w1e.md has the areas one section each.
//
// Area 65 is a world map (WorldMap_Records' record 3): its code is area 45's
// (docs/area_w1b.md section 5, itself area 16's), instruction for instruction
// over its own tables, but for the plate's animation bank (0x53, area 45's
// 0x3B) and the place hook's name sets (two sets of five items over five text
// rows, area 45's three of four over four); its region label reads area 45's
// cell 0x803584. Its block also holds Area65_Record8Move, the record +8
// effect's state 2 of all ten world maps. Area 67 has three choices and
// nineteen handlers: messages, effects spawned at the party members, nudges of
// the first two field objects, a music fade, a map byte and a state reset; and
// object trigger 31 (placed in its block by address) arms a mode tail.
//
// Every one is a faithful replacement: no DIVERGENCE.md entry is owed. The
// dispatchers through area 65's .data state tables abort past the table where
// the original would call whatever the next dwords hold (the owner's rule for
// an unchecked index, round9 doc section 6; no route reaches it). Every call
// goes through the harness (AH_CALL / AH_AT), so the start-up fuzz can stand
// recorders in for the callees.
#include "game/area_w1e.h"

#include <cstdint>
#include <cstring>

#include "bof3/symbols.gen.h"
// Rebound 2026-09-28 (round ten's cleanup, docs/round-10-cleanup.md item 1):
// every constant here whose target is ours reads bof3::addr::<Name>. The values
// are unchanged - the fuzz keys on them - and the comments' "nobody owns" is as
// of the wave that wrote them.
#include "game/area_harness.h"
#include "game/area_w1e_callees.h"
#include "game/move_script_bytes.h"
#include "game/record_views.h"
#include "game/records.h"
#include "game/world_map.h"
#include "hook/detour.h"
#include "hook/log.h"

namespace {

namespace at = area_w1e::at;
using U = std::uint32_t;
using move_script::At;
using move_script::Long;
using move_script::SetLong;
using move_script::Word;

unsigned char* Cur() { return Sprite_Current; }
void AddLong(unsigned char* p, U v) { SetLong(p, static_cast<std::int32_t>(static_cast<U>(Long(p)) + v)); }

}  // namespace

// ===========================================================================
// Area 65: the world map's field hook (WorldMap_FieldHooks entry 3)
// ===========================================================================

// ===========================================================================
// Area 65: the place plate (world-map record 3 +0: effect kind 0)
// ===========================================================================

// ===========================================================================
// Area 65: the HUD task (record 3 +0xC: effect kind 0x58), the dial frame's
// slide and the region box
// ===========================================================================

// ===========================================================================
// Area 65: the record's slots +8 and +4 (effect kinds 0x16 and 0xE)
// ===========================================================================

// ===========================================================================
// Area 65: the drift layer (record 3 +0x10)
// ===========================================================================

// ===========================================================================
// Area 67: three choices
// ===========================================================================

// ===========================================================================
// Area 67: nineteen handlers (Area67_Handlers 0x606F20; PSX 0x801F3EC4..)
// ===========================================================================

// ===========================================================================

void AreaW1e_Inject() {
    if (bof3::WantsShadow("area_w1e")) area_w1e::SelfTest();
}
