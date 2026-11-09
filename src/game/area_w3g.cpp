// World 3's areas 148..151: the code of the PSX's BIN/WORLD03/AREA148..151.EMI
// compiled into the exe at 0x4223A0..0x4249CF - 56 functions, each read to its
// last instruction with capstone (2026-09-28) and taken through the area
// harness (area_harness.h). Round ten group AR3G; docs/area_w3g.md has the
// areas one section each. Area 147 has no code in the band.
//
// Area 148: two handlers that open a message by the first party member of a
// set, two choices, tail kind 31 (a drop-in, two effect-kind-0x13 flashes, a
// light switch that toggles story flag 0x6A and the CLUT), its arrive hook,
// object trigger 17, a cell hook (the switch), and effect kind 0x7E: a
// searchlight that sweeps ahead, pauses at random, turns the party members in
// its cells, and draws itself (a line, a fan of eight triangles and two
// Gouraud quads, x87 through the CRT's _ftol) while flag 0x6A is set.
// Area 149: handler 1's four states (three camera-distance thresholds, two
// dust effects of kind 0x37 about the kind-2 sprite), a view shift the chapter
// code calls (0x56ABC4), tail kind 33 (the view shifted back, then a change of
// area) and its init. Area 150: three choices, tail kind 58 and its step hook.
// Area 151 is a world map (WorldMap_Records record 9): area 87's code
// instruction for instruction over its own tables but for its place hook,
// which has no cell or name-set search (one name set, message 0x16), and no
// plate start of its own (its plate state 0 is area 152's, 0x424BA0). Its 24
// functions are one body here over a WorldMapTables (area_w3g_callees.h); the
// body is area_w3a.cpp's, copied (one shared body is the rebinding pass's).
//
// Every one is a faithful replacement: no DIVERGENCE.md entry is owed. The
// dispatchers through an area's .data state table abort past the table where
// the original would call whatever the next dwords hold (the owner's rule for
// an unchecked index, round9 doc section 6); reads and writes by an unchecked
// index that stay in mapped memory are kept, as the original makes them
// (docs/area_w3g.md, the latent defects). Every call goes through the harness
// (AH_CALL / AH_AT), so the start-up fuzz can stand recorders in for the
// callees; the group's own callees are called the same way, so each function
// is fuzzed alone.
#include "game/area_w3g.h"

#include <cstdint>
#include <cstring>

#include "bof3/symbols.gen.h"
#include "game/area_harness.h"
#include "game/area_w3g_callees.h"
#include "game/move_script_bytes.h"
#include "game/record_views.h"
#include "game/record_views.h"
#include "game/records.h"
#include "game/world_map.h"
#include "hook/detour.h"
#include "hook/log.h"

namespace {

namespace at = area_w3g::at;
using U = std::uint32_t;
using area_harness::Handler;
using move_script::At;
using move_script::Long;
using move_script::SetWord;
using move_script::Word;

unsigned char* Cur() { return Sprite_Current; }
unsigned char* Bank() { return At(::bof3::Img(Cond_Flags_addr) + 0xa0); }
unsigned char* Bank904000() { return At(::bof3::Img(Cond_Flags_addr) + 0x70); }
unsigned char* EffectAt(U slot) { return At(::bof3::Img(Effect_Objects_addr) + slot * at::kEffectStride); }
// The high word of a 16.16 position, as the hooks read it (a word at +2).
std::uint16_t High(long v) { return static_cast<std::uint16_t>(static_cast<U>(v) >> 16); }
void SetMessage(unsigned id) { SetWord(At(::bof3::Img(at::kMessage.v)), id); }
unsigned char& B(U address) { return move_script::At(address)[0]; }
void Disarm() {
    AreaW0a_TailKind = 0;
    AreaW0c_TailState = 0;
}
// The tails' u16 frame counter less one: true when it reached 0.
bool TimerDone() {
    const auto t = static_cast<std::uint16_t>(Word(At(::bof3::Img(at::kTailTimer.v))) - 1u);
    SetWord(At(::bof3::Img(at::kTailTimer.v)), t);
    return t == 0;
}

// A state handler read from an area's .data table in place, as the
// originals' `jmp [index * 4 + table]`: the index is not checked there. Ours
// aborts past the table (what follows is the next table, or data).
Handler StateEntry(const char* who, const char* what, U table, unsigned count, unsigned index) {
    if (index >= count)
        bof3::Fatal("%s_%s: state %u is past its %u-entry table 0x%X", who, what, index, count, static_cast<unsigned>(table));
    return reinterpret_cast<Handler>(static_cast<std::uintptr_t>(static_cast<U>(Long(At(table + index * 4u)))));
}

// ---- x87 as the originals have it -------------------------------------------

}  // namespace

// ===========================================================================
// Area 148 (descriptor 0x635388): its choice array +0x34 starts two dwords
// before its handler array +0x3C (choices 2..7 are handlers 0..5); choices 2..5
// / handlers 0..3 are other bands' (0x42C8A0, Area146_ClearFlag46,
// Area146_ToExtraObject0, 0x425DB0), its init the bare ret 0x437CC0.
// ===========================================================================

// ===========================================================================
// Area 149 (descriptor 0x6361B8; handler 0 is Area146_ToExtraObject0)
// ===========================================================================

// ===========================================================================
// Area 150 (descriptor 0x636A18: a choice table only, no handlers, no init)
// ===========================================================================

// ===========================================================================
// The world-map body (area 151's; area_w3a.cpp's copy for area 115, itself
// area_w2b.cpp's for areas 87 and 88, area 45's code, docs/area_w1b.md
// section 5; area 16's in docs/area_w0b.md; area 33's in docs/worldmap_area.md
// sections 1..4)
// ===========================================================================

// ===========================================================================
// Area 151: the world map's tenth copy (WorldMap_Records record 9)
// ===========================================================================

void AreaW3g_Inject() {
    if (bof3::WantsShadow("area_w3g")) area_w3g::SelfTest();
}
