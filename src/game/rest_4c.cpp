// Group R4C of round fourteen (wave four): 60 functions at 0x459EE0..0x45C3F3 -
// the cut's 60 rows for R4C (analysis/round14_cut.tsv), none added, none
// dropped, each read to its last instruction with capstone (2026-10-05) and
// fuzzed through the scenario harness's field mode (rest_4c_fuzz.cpp).
// docs/rest_4c.md has them one row each.
//
// The field hook's state 2 (R4B's table 0x652A70 by 0x9039F4) jumps through
// R4B's 0x652A84 by the game byte 0x9039F5; its entries 0 and 1 are this
// group's CommuHiLo_Run and CommuHitBlow_Run, each a dispatcher on the phase
// byte 0x939A3E through its own Phases table, whose entries dispatch on the
// state byte 0x939A40 and (two levels down) the step byte 0x939A3F. The PSX
// twins lie in the COMMU overlays (analysis/pairs_propagated.json); none has
// a name in the sibling.
//
// Every one is a faithful replacement. Where the original indexes a .data
// table, the stake's digits, the cards or the guess records by a byte it never
// bounds, ours aborts with a message before the access (round9 doc section 6;
// docs/rest_4c.md section 7 has the one policy). Every call goes through the
// harness (SH_CALL / SH_AT), so the start-up fuzz can stand recorders in for
// the callees; every cell is re-read after a call where the original re-reads
// it, and a callee's answer is taken into a local before the next read.
#include "game/rest_4c.h"

#include <cstdint>
#include <cstring>

#include "bof3/symbols.gen.h"
#include "game/move_script_bytes.h"
#include "game/labels.h"
#include "game/rest_4c_callees.h"
#include "game/scenario_harness.h"
#include "game/text_advance.h"
#include "hook/detour.h"
#include "hook/log.h"

namespace {

namespace at = rest_4c::at;
using U = std::uint32_t;
using move_script::At;

signed char S8(U address) { return static_cast<signed char>(At(address)[0]); }

}  // namespace

// ==== the shared helpers ================================================================

// ==== the first game: CommuHiLo ===========================================================

// ==== the second game: CommuHitBlow ========================================================

// The panels the second game draws under its boxes, in the original's orders.
void Rest4C_Inject() {
    if (bof3::WantsShadow("rest_4c")) rest_4c::SelfTest();
}
