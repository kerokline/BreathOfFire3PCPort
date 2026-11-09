// World 2's areas 85..88: the code of the PSX's BIN/WORLD02/AREA085..088.EMI
// compiled into the exe at 0x40F720..0x411EFF - 66 functions (the band's 68
// less WorldMap_PinSprite 0x4112A0, round seven's, and WorldMap_FrameWait
// 0x411310, round eight's, which lie in area 88's block), each read to its
// last instruction with capstone (2026-09-28) and taken through the area
// harness (area_harness.h). Round ten group AR2B; docs/area_w2b.md has the
// areas one section each.
//
// Area 85: ten handlers (a kind-2 walk, an object that follows party member
// 2, two effect-0x47 spawns, a Field_Slots script start and release, a height
// nudge, a sound) and an init that darkens CLUT row 6 through a brightness
// shift an engine effect state (0x478649) also calls. Area 86: a cell hook
// whose three floor switches toggle story flags 8..10 unless a party member
// stands on the switch's gate cells, and an init that closes two map cells
// before chapter 13. 0x40FC40 is areas 65's and 87's init (in area 86's
// block by address). Areas 87 and 88 are world maps (WorldMap_Records records
// 4 and 5): area 45's code (docs/area_w1b.md section 5, itself area 16's)
// instruction for instruction over their own tables - area 87 with two
// constants different (the plate's animation bank 0x156, the region label's
// offset cell 0x803580), area 88 with the same two (bank 0x157) and a place
// hook whose name sets hold eleven items and fill eleven text rows where
// area 45's hold four. So both copies are one body of code here, over a
// WorldMapTables each (area_w2b_callees.h), with a named entry per address.
//
// Every one is a faithful replacement: no DIVERGENCE.md entry is owed. The
// dispatchers through an area's .data state table abort past the table where
// the original would call whatever the next dwords hold, and area 85's effect
// spawns abort on a slot past Effect_Objects' twenty (the owner's rule for an
// unchecked index, round9 doc section 6; no route reaches either). Every call
// goes through the harness (AH_CALL / AH_AT), so the start-up fuzz can stand
// recorders in for the callees.
#include "game/area_w2b.h"

#include <cstdint>
#include <cstring>

#include "bof3/symbols.gen.h"
// Rebound 2026-09-28 (round ten's cleanup, docs/round-10-cleanup.md item 1):
// every constant here whose target is ours reads bof3::addr::<Name>. The values
// are unchanged - the fuzz keys on them - and the comments' "nobody owns" is as
// of the wave that wrote them.
#include "game/area_harness.h"
#include "game/area_w2b_callees.h"
#include "game/move_script_bytes.h"
#include "game/record_views.h"
#include "game/record_views.h"
#include "game/records.h"
#include "game/world_map.h"
#include "game/event_script.h"   // DIV-0191: g_mountain_passes
#include "hook/detour.h"
#include "hook/log.h"

namespace {

namespace at = area_w2b::at;
using U = std::uint32_t;
using move_script::At;
using move_script::Word;

unsigned char* Cur() { return Sprite_Current; }

}  // namespace

// ===========================================================================
// Area 85
// ===========================================================================

// ===========================================================================
// Area 86
// ===========================================================================

// ===========================================================================
// Areas 87 and 88: the world-map copies (area 45's code, docs/area_w1b.md
// section 5; area 16's in docs/area_w0b.md; area 33's in
// docs/worldmap_area.md sections 1..4)
// ===========================================================================

// ===========================================================================
// Area 87: the world map's fifth copy (WorldMap_Records record 4)
// ===========================================================================

// ===========================================================================
// Area 88: the world map's sixth copy (WorldMap_Records record 5; the
// world-map route plays it)
// ===========================================================================

// ===========================================================================

void AreaW2b_Inject() {
    if (bof3::WantsShadow("area_w2b")) area_w2b::SelfTest();
}
