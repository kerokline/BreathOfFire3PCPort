// World 3's areas 120 and 121: the code of the PSX's BIN/WORLD03/AREA120..121
// .EMI compiled into the exe at 0x41A9D0..0x41C88C - 54 functions, each read to
// its last instruction with capstone (2026-09-28) and taken through the area
// harness (area_harness.h). Round ten group AR3B; docs/area_w3b.md has the
// areas one section each.
//
// Area 120: three choice handlers (two set movement-script variable 3, one
// starts run 0xD or answers message 0x35). Area 121: three handlers of its own
// (a two-state camera-distance ramp, a member reset that starts run 4, an effect
// 0xBA spawn); then the world map's ninth copy (WorldMap_Records record 8):
// area 87's code (docs/area_w2b.md section 4, itself area 45's) instruction for
// instruction over area 121's tables, but for three functions - a field hook
// that opens message (the place's index in a list of four) + 1 where area 87's
// names a place's items, the plate's animation bank 0x158, and a drift layer of
// two squares (0x295 bytes) where area 87's draws a square and a grid of map
// items. Then two engine-reached machines area 121's block holds: leader state
// 12 (Field_LeaderStates[12] 0x52FE90 jumps here in every area but 104, which
// has its own copy at 0x415020) - walk, turn, push an object, step off one, the
// menu and request buttons - and effect kind 0x5C (Effect_KindHandlers[0x5C]
// EffectKind5C_Run 0x462B60, likewise but for area 104's 0x415780): an object that follows the
// leader, turns with it, draws a two-row gauge, and four rings that rise from
// the leader. Last, EffectKind18_States[70] (a full-width band at the top of
// the screen) and Field_ObjectTriggers id 38 (arms the world map's field hook).
//
// Every one is a faithful replacement: no DIVERGENCE.md entry is owed. The
// dispatchers through a .data state table abort past the table, where the
// original would jump through whatever the next dwords hold, and effect kind
// 0x5C's start aborts on an area number past Area_Descriptors or a null
// descriptor (the owner's rule, round9 doc section 6: ours aborts where the
// original would fault; no route reaches any). The out-of-table reads and
// writes that land in mapped memory are reproduced as the original makes them
// (docs/area_w3b.md section 7). Every call goes through the harness (AH_CALL /
// AH_AT), so the start-up fuzz can stand recorders in for the callees.
#include "game/area_w3b.h"

#include <cstdint>
#include <cstring>

#include "bof3/symbols.gen.h"
#include "game/area_harness.h"
#include "game/area_w3b_callees.h"
#include "game/move_script_bytes.h"
#include "game/record_views.h"
#include "game/record_views.h"
#include "game/records.h"
#include "game/world_map.h"
#include "game/widescreen.h"
#include "hook/detour.h"
#include "hook/log.h"

namespace {

namespace at = area_w3b::at;
using move_script::At;
using move_script::Word;

}  // namespace

// ===========================================================================
// Area 120 (its descriptor 0x623648: a choice table only)
// ===========================================================================

// ===========================================================================
// Area 121: its handlers (its descriptor 0x6245D8's +0x3C, Area121_Handlers)
// ===========================================================================

// ===========================================================================
// Area 121: the world map's ninth copy (area 87's code, docs/area_w2b.md
// section 4; area 45's in docs/area_w1b.md section 5; area 33's in
// docs/worldmap_area.md sections 1..4)
// ===========================================================================

// ===========================================================================
// Area 121: leader state 12 (Field_LeaderStates[12] 0x52FE90: area 104 to
// its own 0x415020, every other area here)
// ===========================================================================

// ===========================================================================
// Area 121: effect kind 0x5C (Effect_KindHandlers[0x5C] EffectKind5C_Run 0x462B60: area 104 to
// its own 0x415780, every other area here)
// ===========================================================================

// ===========================================================================
// Area 121: an effect-kind-0x18 state and an object trigger
// ===========================================================================

// ===========================================================================

void AreaW3b_Inject() {
    if (bof3::WantsShadow("area_w3b")) area_w3b::SelfTest();
}
