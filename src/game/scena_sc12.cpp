// Chapter 12's first block of scenario code, 0x55E4E0..0x561DB0 (round ten
// group SC12). docs/scena_sc12.md.
//
//   - The vtable at 0x6616E0 (0x662C80's entry 12): slot 0 Scena12_Frame, a
//     tail jump through Scena12_States on the state byte; slot 1
//     Scena12_ObjectTrigger, a call through Scena12_Objects on the object's
//     +0x86; slots 2 and 3 the step and arrive hooks (x, z) answering in al;
//     slot 4 Scena12_CellHook (a, b), the record search 0x56D800 over
//     Scena12_CellRecords and a tail jump through Scena12_CellHooks.
//   - State 1 Scena12_EnterArea (every way out stores state 2); state 2
//     Scena12_Run, a tail jump through Scena12_Runs on MoveScript_Var7; the
//     runs 1, 2, 4..9 are each a switch on the step byte 0x8034E5 whose cases
//     wait on a counter, the request byte or the wait word, do one thing and
//     set the next step. State 0 (0x5646B0) lies in group SC13's block.
//
// Every call goes through the harness (SH_CALL / SH_AT), so the start-up fuzz
// can stand recorders in for ours as for the originals' copies. The tables
// are read in place and their entries called directly, as Scena01_Frame and
// magic_s16's dispatches do; the fuzz swaps them for recorders. No divergence:
// each function is a faithful replacement, except that a dispatcher whose
// index lies outside its table (a negative state or run, or one reading the
// next table) aborts where the original would jump through its neighbour -
// the project's rule for an index past a table (round nine, section 6).
#include "game/scena_sc12.h"

#include <cstdint>

#include "bof3/symbols.gen.h"
#include "game/record_views.h"
#include "game/scena_sc12_callees.h"
#include "game/scenario_harness.h"
#include "hook/detour.h"
#include "hook/log.h"

namespace {

namespace at = scena_sc12::at;

}  // namespace

// ===========================================================================
// The vtable's slots

// ===========================================================================
// Scena12_Objects: the handlers of the object trigger (entries 6, 9..14; the
// rest a bare ret). Each starts a run: ScriptFlags_Set40, the counters (some
// of them) 0, the step and the run.

// ===========================================================================
// State 1: once per area entered

// ===========================================================================
// State 2's runs: each a switch on the step byte 0x8034E5 (unsigned); a step
// no case holds does nothing.

void ScenaSc12_Inject() {
    if (bof3::WantsShadow("scena_sc12")) scena_sc12::SelfTest();
}
