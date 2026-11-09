// World 1's areas 42..47: the code of the PSX's BIN/WORLD01/AREA042..047.EMI
// compiled into the exe at 0x406650..0x408FE7 - 55 functions (the band's 56
// less WorldMap_DrawNeedle 0x408530, round seven's, which lies in area 45's
// block), each read to its last instruction with capstone (2026-09-27) and
// taken through the area harness (area_harness.h). Round ten group AR1B;
// docs/area_w1b.md has the areas one section each.
//
// Area 42 runs a timed round: its handlers set four story flags, a mode tail
// (kind 7) counts 900 frames down on screen and a step hook calls it off.
// Area 43 an object's tint fade and two small handlers. Area 44 two gates of
// map cells a cell hook's four switches turn, through two mode tails (kinds
// 8 and 9, armed through a register: the tool's second known gap). Area 45
// is a world map (WorldMap_Records' record 2): its code is area 16's
// (docs/area_w0b.md), instruction for instruction over its own tables, but
// for the plate's animation bank and the region label's offset cell. Area 46
// a party drop-in tail (kind 11) and its step hook; area 47 two handlers.
//
// Every one is a faithful replacement: no DIVERGENCE.md entry is owed. The
// dispatchers through an area's .data state table abort past the table where
// the original would call whatever the next dwords hold (the owner's rule for
// an unchecked index, round9 doc section 6; no route reaches it). Every call
// goes through the harness (AH_CALL / AH_AT), so the start-up fuzz can stand
// recorders in for the callees.
#include "game/area_w1b.h"

#include <cstdint>
#include <cstring>
#include <initializer_list>

#include "bof3/symbols.gen.h"
// Rebound 2026-09-28 (round ten's cleanup, docs/round-10-cleanup.md item 1):
// every constant here whose target is ours reads bof3::addr::<Name>. The values
// are unchanged - the fuzz keys on them - and the comments' "nobody owns" is as
// of the wave that wrote them.
#include "game/area_harness.h"
#include "game/area_w1b_callees.h"
#include "game/move_script_bytes.h"
#include "game/record_views.h"
#include "game/records.h"
#include "game/world_map.h"
#include "hook/detour.h"
#include "hook/log.h"

namespace {

namespace at = area_w1b::at;
using U = std::uint32_t;
using area_harness::Handler;
using move_script::At;
using move_script::Long;
using move_script::SetLong;
using move_script::Word;

unsigned char* Cur() { return Sprite_Current; }
std::int32_t S16(const unsigned char* p) { return static_cast<std::int16_t>(Word(p)); }
unsigned char* Bank() { return At(::bof3::Img(Cond_Flags_addr) + 0xa0); }
unsigned char& B(U address) { return move_script::At(address)[0]; }
signed char TailState() { return AreaW0c_TailState; }
unsigned char* EffectAt(unsigned slot) { return rv::Bytes(&Effect_Objects[slot]); }
unsigned char* ActiveMember() { return Field_ActiveMember; }
void AddLong(unsigned char* p, U v) { SetLong(p, static_cast<std::int32_t>(static_cast<U>(Long(p)) + v)); }

// A state handler read from an area's .data table in place, as the
// originals' `jmp [index * 4 + table]`: the index is not checked there. Ours
// aborts past the table (what follows is the next table, or data).
Handler StateEntry(const char* who, U table, unsigned count, unsigned index) {
    if (index >= count)
        bof3::Fatal("%s: state %u is past its %u-entry table 0x%X", who, index, count, static_cast<unsigned>(table));
    return reinterpret_cast<Handler>(static_cast<std::uintptr_t>(static_cast<U>(Long(At(table + index * 4u)))));
}

}  // namespace

// ===========================================================================
// Area 42: a timed round - four story flags 0x12..0x15 set against a
// 900-frame countdown (mode tail kind 7)
// ===========================================================================

// ===========================================================================
// Area 43
// ===========================================================================

// ===========================================================================
// Area 44: two gates of map cells, four switches
// ===========================================================================

// ===========================================================================
// Area 45: the world map's field hook (WorldMap_FieldHooks entry 2)
// ===========================================================================

// ===========================================================================
// Area 45: the place plate (world-map record 2 +0: effect kind 0)
// ===========================================================================

// ===========================================================================
// Area 45: the HUD task (record 2 +0xC: effect kind 0x58), the dial frame's
// slide and the region box
// ===========================================================================

// ===========================================================================
// Area 45: the record's slots +8 and +4 (effect kinds 0x16 and 0xE)
// ===========================================================================

// ===========================================================================
// Area 45: the drift layer (record 2 +0x10)
// ===========================================================================

// ===========================================================================
// Area 46: a party drop-in (mode tail kind 11) and its step hook
// ===========================================================================

// ===========================================================================
// Area 47
// ===========================================================================

// ===========================================================================

void AreaW1b_Inject() {
    if (bof3::WantsShadow("area_w1b")) area_w1b::SelfTest();
}
