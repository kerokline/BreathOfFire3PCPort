// Round fourteen's group R1F - the 49 functions the cut lists for R1F
// (analysis/round14_cut.tsv), 0x523ED0..0x52899B, each read with capstone to
// its last instruction (docs/rest_1f.md section 1): party sets 16, 17 and 18's
// field actions, a raised sprite's cell ahead and its helpers, and the
// leader's state 9's stage 0.
//
// Every call out goes through the scenario harness (SH_CALL, or
// scenario_harness::Call on a function pointer), so the start-up fuzz can
// stand recorders in for ours as for the originals' copies; the .data tables
// the dispatchers jump through are read in place (the fuzz swaps their cells
// for recorders). No divergence: each is a faithful replacement. Where an
// original indexes a table or a record array by a byte it does not bound -
// the dispatchers' state bytes and form words, an object index from
// Sprite_ObjectAt, an effect index - ours aborts with a message past the
// table's or the array's end (docs/rest_1f.md section 7).
#include "game/rest_1f.h"

#include <cstdint>

#include "bof3/symbols.gen.h"
#include "game/move_script_bytes.h"
#include "game/record_views.h"
#include "game/records.h"
#include "game/rest_0a.h"
#include "game/scenario_harness.h"
#include "hook/detour.h"
#include "hook/log.h"

namespace {

using U = std::uint32_t;
using move_script::At;
using move_script::SetWord;
using Handler = void (__cdecl*)();

// kPanelPose: now Fish_Nibbling (symbols.toml): rest_1g's "a fish is nibbling", its one reader Fish_Swim; this file only clears it
// kPanelCounter: now Effect1e_Choice (symbols.toml).
// kPanelBlink: now Effect1e_Blink (symbols.toml). FieldPanel_DrawBlink's switch

unsigned char* Sc() { return Sprite_Current; }
std::uint16_t W16(unsigned v) { return static_cast<std::uint16_t>(v); }

// A dispatcher: the table's entry by `index`, called as the original jumps
// to it - a Fatal past the table's own entries, where the original jumps
// through the dword after (docs/rest_1f.md section 7).
void DispatchRest1f(const char* who, const char* by, const unsigned long* table, unsigned count, unsigned index) {
    if (index >= count)
        bof3::Fatal("%s: %s is %u, past the %u entries of 0x%X - the original jumps through the dword after "
                    "(docs/rest_1f.md section 7)",
                    who, by, index, count, static_cast<unsigned>(::bof3::VaOf(Low32(table))));
    reinterpret_cast<Handler>(static_cast<std::uintptr_t>(table[index]))();
}
void ByStateRest1f(const char* who, const unsigned long* table, unsigned count) {
    DispatchRest1f(who, "Sprite_Current +2", table, count, Sc()[2]);
}
void ByStepRest1f(const char* who, const unsigned long* table, unsigned count) {
    DispatchRest1f(who, "Sprite_Current +3", table, count, Sc()[3]);
}

// --- the shapes three sets share (docs/rest_1f.md section 1.2) -------------------------------

}  // namespace

// ===========================================================================
// The dispatchers (docs/rest_1f.md section 3: each table's entries)
// ===========================================================================

// ===========================================================================
// The state handlers
// ===========================================================================

// ===========================================================================
// A raised sprite's cell ahead (Field_CellAhead's other half)
// ===========================================================================

// ===========================================================================
// The leader's state 9, stage 0
// ===========================================================================

void Rest1F_Inject() {
    if (bof3::WantsShadow("rest_1f")) rest_1f::SelfTest();
}
