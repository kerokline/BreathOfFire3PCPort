// Party sets 9 to 12's field actions - round fourteen, wave one, group R1D:
// the 46 functions 0x520E10..0x5226C1 of the cut (analysis/round14_cut.tsv),
// each read with capstone to its last instruction (docs/rest_1d.md section 1).
// 27 are dispatchers jumping through a .data state table; 19 are state
// handlers and the two cell probes they call. Three bodies recur unchanged in
// the band (the side-or-slope start, the resolve and the cell pickup, once per
// set 9, 10 and 11) and one twice (set 10's and 11's kind-0x30 start): each
// copy is its own function, every copy compared against its own original.
//
// Every call out goes through the scenario harness (SH_CALL), so the start-up
// fuzz can stand recorders in for ours as for the originals' copies; a table's
// handler is called by the address the table holds. No divergence: each is a
// faithful replacement. Where the original indexes a table past its own count
// with a byte no handler writes, or a record array by an answer no callee
// gives, ours aborts with a message (section 5); the one index play reaches
// (PartyAction_WaitEffectDone's +0xB of 0xFF) is read in place as the
// original reads it.
#include "game/rest_1d.h"

#include <cstdint>
#include <cstdlib>

#include "bof3/symbols.gen.h"
#include "game/move_script_bytes.h"
#include "game/record_views.h"
#include "game/records.h"
#include "game/rest_0a.h"
#include "game/rest_1d_callees.h"
#include "game/scenario_harness.h"
#include "hook/detour.h"
#include "hook/log.h"

namespace {

namespace at = rest_1d::at;

// kEffects: now Effect_Objects (symbols.toml). Effect_Objects, 20 records

// --- the three bodies sets 9, 10 and 11 share ----------------------------------

}  // namespace

// ===========================================================================
// The dispatchers. Each is `mov ecx, [Sprite_Current]; xor eax, eax; mov al /
// ax, [ecx + k]; jmp [eax * 4 + table]` (0x12 / 0x13 bytes): the index zero-
// extended, unchecked; ours aborts past the table's count (Jump).
// ===========================================================================

// ===========================================================================
// The state handlers
// ===========================================================================

void Rest1D_Inject() {
    if (bof3::WantsShadow("rest_1d")) rest_1d::SelfTest();
}
