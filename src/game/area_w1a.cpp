// World 1's areas 38..41: the PSX's BIN/WORLD01/AREA038..041.EMI compiled into
// the exe at 0x4053B0..0x406650 (Area_Descriptors entries 38..41). Round ten,
// group AR1A: the band's 48 functions, none ours before, each read to its last
// instruction with capstone (2026-09-28) and taken through the area harness
// (area_harness.h). docs/area_w1a.md.
//
// Every one is a faithful replacement: no DIVERGENCE.md entry is owed. The two
// dispatchers through an area's .data state table abort past the table where
// the original would jump through whatever the next dwords hold (the owner's
// rule for an unchecked index, round9 doc section 6; no route reaches it).
// Area40_TileLit's record walk runs on for ever on a step of 0 or one that
// overshoots, as the original does (docs/area_w1a.md section 6). Every call
// goes through the harness (AH_CALL / AH_AT), so the start-up fuzz can stand
// recorders in for ours as for the originals' copies.
#include "game/area_w1a.h"

#include "game/draw_pool.h"

#include <cstdint>

#include "bof3/symbols.gen.h"
#include "game/area_harness.h"
#include "game/area_w1a_callees.h"
#include "game/move_script_bytes.h"
#include "game/record_views.h"
#include "game/record_views.h"
#include "game/records.h"
#include "hook/detour.h"
#include "hook/log.h"

namespace {

namespace at = area_w1a::at;
using area_harness::Mem;
using move_script::Long;
using move_script::SetLong;
using move_script::SetWord;
using move_script::Word;
using U = std::uint32_t;

using Handler = void (__cdecl*)();

unsigned char& B(U address) { return *Mem(address); }
void AddWord(unsigned char* p, int v) { SetWord(p, static_cast<unsigned>(Word(p) + v)); }
void AddLong(unsigned char* p, U v) { SetLong(p, static_cast<std::int32_t>(static_cast<U>(Long(p)) + v)); }
unsigned char* Ptr(U cell) { return reinterpret_cast<unsigned char*>(static_cast<std::uintptr_t>(static_cast<U>(Long(Mem(cell))))); }

// A state handler read from an area's .data table in place, as the
// originals' `jmp [index * 4 + table]`: the index is not checked there. Ours
// aborts past the table (what follows is data).
Handler StateEntry(const char* who, U table, unsigned count, unsigned index) {
    if (index >= count)
        bof3::Fatal("%s: state %u is past its %u-entry table 0x%X", who, index, count, static_cast<unsigned>(table));
    return reinterpret_cast<Handler>(static_cast<std::uintptr_t>(static_cast<U>(Long(Mem(table + index * 4u)))));
}

void SetMessage(unsigned id) { SetWord(Mem(::bof3::Img(at::kMessage.v)), id); }

void SetByte(U x, U z, unsigned value) { AH_CALL(AreaMap_SetByte)(x, z, value); }

}  // namespace

// ===========================================================================
// Area 38 (descriptor 0x5F2208): four handlers.
// ===========================================================================

// ===========================================================================
// Area 39 (descriptor 0x5F3E60): eleven handlers of its own and a two-state
// machine.
// ===========================================================================

// ===========================================================================
// Area 40 (descriptor 0x5F4BD8): ten choices (two of them the handlers), an
// init, and a 4 x 4 puzzle - a mode tail, an arrive hook and five helpers.
// ===========================================================================

// ===========================================================================
// Area 41 (descriptor 0x5F5F10): fifteen choices (the last five the
// handlers), a four-state machine, two object triggers.
// ===========================================================================

void AreaW1a_Inject() {
    if (bof3::WantsShadow("area_w1a")) area_w1a::SelfTest();
}
