// World 2's areas 104..106: the code of the PSX's BIN/WORLD02/AREA104..106.EMI
// compiled into the exe at 0x4146C0..0x4168DF - 52 functions (the band's 53
// less WorldMapHud_BoxWait 0x414BB0, round eight's, which lies in area 104's
// block), each read to its last instruction with capstone (2026-09-28) and
// taken through the area harness (area_harness.h). Round ten group AR2E;
// docs/area_w2e.md has the areas one section each.
//
// Area 104 is WorldMap_Records record 6: eighteen of its functions are area
// 87's world-map code (docs/area_w2b.md section 4, itself area 45's and area
// 16's) instruction for instruction over area 104's tables - the plate's
// states 1..4, the HUD's frame and box machines, the three draws - with no
// constant of the copies' differing (no plate bank: its plate state 0 is
// 0x41ACD0, outside the band; the label cell is 0x803580 as areas 87's and
// 88's). Its place hook is its own (one message), and record 6 has no +4, +8
// or +0x14. The rest of area 104 is its own: a leader controller the engine
// runs as leader state 12 in area 104 (0x52FE90 jumps here when
// Game_AreaNumber is 0x68, to area 121's copy 0x41B9D0 otherwise) - walk,
// turn, step, a charge held on a button - with two functions that run only in
// area 121 (their first test is Game_AreaNumber 0x79); effect kind 0x5C (a
// companion that follows the leader and draws the charge's gauge; area 104's
// by the same area test in EffectKind5C_Run 0x462B60); effect kind 0x6A (a countdown drawn with
// a panel and a pulsing marker, which sends the party to area 0x79 when it
// runs out); mode-tail kind 40; a 4-bit minimap of the area's cells built at
// entry when key item 0xA is held; object trigger 36. Area 105: a tail, a
// step hook and an init. Area 106: three handlers placing an object by story
// flags, a tail that drops a party member in and moves the flags, a step hook
// that arms it on five cells.
//
// Every one is a faithful replacement: no DIVERGENCE.md entry is owed. Where
// the original indexes past a table (the .data state tables, Effect_Objects by
// a slot Effect_FindFree answered "none" for, Sprite_ObjectsExtra by an object
// index of none, the upload queue past its twenty slots, the gauge's two-entry
// stack table, Area_Descriptors past its 200), ours aborts loudly (the
// owner's rule for an unchecked index, round9 doc section 6; no route reaches
// one). Every call goes through the harness (AH_CALL / AH_AT / Phase), so the
// start-up fuzz can stand recorders in for the callees.
#include "game/area_w2e.h"

#include <bit>
#include <cstdint>
#include <cstring>

#include "bof3/symbols.gen.h"
// Rebound 2026-09-28 (round ten's cleanup, docs/round-10-cleanup.md item 1):
// every constant here whose target is ours reads bof3::addr::<Name>. The values
// are unchanged - the fuzz keys on them - and the comments' "nobody owns" is as
// of the wave that wrote them.
#include "game/area_harness.h"
#include "game/area_w2e_callees.h"
#include "game/move_script_bytes.h"
#include "game/record_views.h"
#include "game/record_views.h"
#include "game/records.h"
#include "game/world_map.h"
#include "game/widescreen.h"
#include "hook/detour.h"
#include "hook/log.h"

namespace {

namespace at = area_w2e::at;
using U = std::uint32_t;
using move_script::At;

unsigned char* Cur() { return Sprite_Current; }
unsigned char* Bank() { return At(::bof3::Img(Cond_Flags_addr) + 0xa0); }
unsigned char& B(U address) { return *At(address); }

void Set40() { AH_CALL(ScriptFlags_Set40)(); }

}  // namespace

// ===========================================================================
// Area 104: the init and the place hook
// ===========================================================================

// ===========================================================================
// Area 104: the world map's seventh copy (area 87's code, docs/area_w2b.md
// section 4 - the same bodies over area 104's tables)
// ===========================================================================

// ===========================================================================
// Area 104: the leader controller (leader state 12 in area 104)
// ===========================================================================

// ===========================================================================
// Area 104: effect kind 0x5C (Effect_KindHandlers[0x5C] EffectKind5C_Run 0x462B60 jumps here
// when Game_AreaNumber is 0x68, to area 121's 0x41C190 otherwise)
// ===========================================================================

// ===========================================================================
// Area 104: mode-tail kind 40, effect kind 0x6A (the countdown), the minimap,
// object trigger 36
// ===========================================================================

// ===========================================================================
// Area 105
// ===========================================================================

// ===========================================================================
// Area 106
// ===========================================================================

// ===========================================================================

void AreaW2e_Inject() {
    if (bof3::WantsShadow("area_w2e")) area_w2e::SelfTest();
}
