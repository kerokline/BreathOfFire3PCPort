// The band 0x5372E0..0x537F1B - round fourteen, wave two, group R2A: the 19
// functions of the cut (analysis/round14_cut.tsv) and three starts no list
// had (0x537760, 0x537B10, 0x537CE0, each reached by an immediate or a .data
// cell), each read with capstone to its last instruction (docs/rest_2a.md
// section 1). The cut called 18 of them scenario code; none is. Four are
// field-engine helpers (a frame's screen pass, two character-record helpers,
// Area_ObjectHandler); the other 18 are the four object handlers of
// Area_ObjectHandler's fallback table and what they call.
//
// Every call out goes through the scenario harness (SH_CALL / Phase), so the
// start-up fuzz can stand recorders in for ours as for the originals' copies;
// a .data table's handler is called by the address the cell holds. No
// divergence: each is a faithful replacement. Where the original indexes a
// table past its own count (a dispatcher's stack table, the fallback table,
// Area_Descriptors, the amounts table) or an effect record by an index no
// callee gives, ours aborts with a message (section 5); the one index play
// reaches - a sprite's +0xB of 0xFF, Effect_FindFree's "none free" - is read
// in place as the original reads it (section 6).
#include "game/rest_2a.h"

#include <cstdint>

#include "bof3/symbols.gen.h"
#include "game/move_script_bytes.h"
#include "game/record_views.h"
#include "game/records.h"
#include "game/rest_2a_callees.h"
#include "game/scenario_harness.h"
#include "hook/detour.h"
#include "hook/log.h"

namespace {

namespace at = rest_2a::at;
using U = std::uint32_t;
using move_script::At;
using move_script::Long;
using move_script::SetLong;
using move_script::SetWord;

}  // namespace

void Rest2A_Inject() {
    if (bof3::WantsShadow("rest_2a")) rest_2a::SelfTest();
}
