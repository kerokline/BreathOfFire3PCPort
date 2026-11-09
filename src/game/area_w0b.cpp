// World 0's areas 16 and 18..26: the code of the PSX's BIN/WORLD00/AREA016.EMI
// and AREA018..026.EMI compiled into the exe at 0x401B80..0x4033F2 - 61
// functions, each read to its last instruction with capstone (2026-09-27) and
// taken through the area harness (area_harness.h). Round ten group AR0B;
// docs/area_w0b.md has the areas one section each.
//
// Area 16 is a world map (WorldMap_Records' record 0): its code is a copy of
// area 33's (docs/worldmap_area.md, docs/world-map.md: every instruction the
// same but the tables' addresses), reached through the record's slots, the
// field-hook table and its own state tables, plus a place-message hook of its
// own and the two record slots +4 / +8 whose area-33 copies are not ours yet.
// Areas 18..26 are small: handlers, choice handlers and two inits.
//
// Every one is a faithful replacement: no DIVERGENCE.md entry is owed. Every
// call goes through the harness (AH_CALL / AH_AT), and every dispatch through
// a .data table reads the table in place, the index unchecked as the
// original's, so the start-up fuzz can stand recorders in for the entries.
#include "game/area_w0b.h"

#include <cstdint>
#include <cstring>

#include "bof3/symbols.gen.h"
// Rebound 2026-09-28 (round ten's cleanup, docs/round-10-cleanup.md item 1):
// every constant here whose target is ours reads bof3::addr::<Name>. The values
// are unchanged - the fuzz keys on them - and the comments' "nobody owns" is as
// of the wave that wrote them.
#include "game/area_harness.h"
#include "game/area_w0b_callees.h"
#include "game/move_script_bytes.h"
#include "game/record_views.h"
#include "game/records.h"
#include "game/world_map.h"
#include "game/event_script.h"   // DIV-0191: g_mountain_passes
#include "hook/detour.h"
#include "hook/log.h"

namespace {

namespace at = area_w0b::at;
using U = std::uint32_t;
using move_script::At;
using move_script::Long;
using move_script::SetWord;
using move_script::Word;

unsigned char* Pointer(U cell) { return At(static_cast<U>(Long(At(cell)))); }
void SetMessage(unsigned id) { SetWord(At(::bof3::Img(at::kMessage.v)), id); }
signed char Choice() { return AreaW0c_ChoiceAnswer; }

}  // namespace

// ===========================================================================
// Area 16: the world map's field hook (WorldMap_FieldHooks entry 0)
// ===========================================================================

// ===========================================================================
// Area 16: the place plate (world-map record 0 +0: effect kind 0)
// ===========================================================================

// ===========================================================================
// Area 16: the HUD task (record 0 +0xC: effect kind 0x58), the dial frame's
// slide and the region box
// ===========================================================================

// ===========================================================================
// Area 16: the record's slots +8 and +4 (effect kinds 0x16 and 0xE)
// ===========================================================================

// ===========================================================================
// Area 16: the drift layer (record 0 +0x10)
// ===========================================================================

// ===========================================================================
// Area 18
// ===========================================================================

// ===========================================================================
// Area 19
// ===========================================================================

// ===========================================================================
// Area 20
// ===========================================================================

// ===========================================================================
// Area 21 (choice handlers)
// ===========================================================================

// ===========================================================================
// Area 22
// ===========================================================================

namespace {
// Areas 22's and 26's choice pattern: message 0xFFFF (none), then the script
// variable 0x903848 = `yes` for choice 0, `no` for choice 1, else unchanged.
void ChoiceSetVar(unsigned char yes, unsigned char no) {
    const signed char choice = Choice();
    SetMessage(0xFFFF);
    if (choice == 0) At(::bof3::Img(at::kScriptVar.v))[0] = yes;
    else if (choice == 1) At(::bof3::Img(at::kScriptVar.v))[0] = no;
}
}  // namespace

// ===========================================================================
// Area 23
// ===========================================================================

// ===========================================================================
// Areas 24 and 25
// ===========================================================================

// ===========================================================================
// Area 26
// ===========================================================================

// ===========================================================================

void AreaW0b_Inject() {
    if (bof3::WantsShadow("area_w0b")) area_w0b::SelfTest();
}
