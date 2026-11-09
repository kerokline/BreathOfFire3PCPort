// Chapter 5's scenario bank, 0x546390..0x54A910 (round ten group SC5).
// docs/scena_sc5.md.
//
//   - The vtable Scena05_Hooks 0x661020 (0x662C80's entry 5): slot 0
//     Scena05_Frame, a tail jump through Scena05_States on the state byte;
//     slot 1 Scena05_ObjectTrigger, a call through Scena05_Objects on the
//     object's +0x86; slot 2 Scena05_StepHook (x, z) answering in al; slot 3
//     Scenario_NoHook (Capcom's, ours already); slot 4 Scena05_CellHook
//     (a, b), the record search 0x56D800 over Scena05_CellRecords and a tail
//     jump through Scena05_CellHooks.
//   - State 1 Scena05_EnterArea (every way out stores state 2); state 2
//     Scena05_Run, a tail jump through Scena05_Runs on MoveScript_Var7; the
//     eighteen runs are each a switch on the step byte 0x8034E5 whose cases
//     wait on a counter, the request byte, the wait word or the message box's
//     done bit, do one thing and set the next step. State 0 (0x5646B0) lies
//     in group SC13's block.
//
// Every call goes through the harness (SH_CALL / SH_AT), so the start-up fuzz
// can stand recorders in for ours as for the originals' copies. The tables
// are read in place and their entries called directly, as Scena12_Frame's
// dispatches do; the fuzz swaps them for recorders. No divergence: each
// function is a faithful replacement, except that a dispatcher whose index
// lies outside its table (a negative state or run, or one reading the next
// table) aborts where the original would jump through its neighbour - the
// project's rule for an index past a table (round nine, section 6).
#include "game/scena_sc5.h"

#include <cstdint>

#include "bof3/symbols.gen.h"
#include "game/record_views.h"
#include "game/scena_sc5_callees.h"
#include "game/scenario_harness.h"
#include "hook/detour.h"
#include "hook/log.h"

namespace {

namespace at = scena_sc5::at;

}  // namespace

// ===========================================================================
// The vtable's slots and the dispatchers

// A step hook's hit: ScriptFlags_Set40, the four counters 0, the step and the
// run; 1.
// ===========================================================================
// Scena05_CellHooks: the cell hook's two entries, (a, b) in place, unread.

// Both: with flag 6 set and flag 9 clear, Flags_Set(the story flags, n),
// ScriptFlags_Set40, counters 2 and 3 0, Sound_PlayEffect(0x202), the step,
// the timer 0; 1. Else 0xFF.
// ===========================================================================
// Scena05_Objects: the handlers of the object trigger (entries 1..8; entry 0
// a bare ret). Each starts a run: ScriptFlags_Set40, the run, the counters
// (from 0 or from 1) and the step 0.

// ===========================================================================
// Two helpers the runs call directly

// ===========================================================================
// State 1

// ===========================================================================
// The runs (Scena05_Runs entries), each a switch on the step 0x8034E5

// ===========================================================================
// Run 16 (0x547920), the chapter's longest: a two-level switch over steps
// 0..0x66, whose cases pick by counter 3 (0x90384B, a code of the party's
// pair set at steps 5 / 6: 1..6 and 0xB..0x10) and by the first member
// 0x904062 (0..6).

// ===========================================================================
// Runs 17..22

void ScenaSc5_Inject() {
    if (bof3::WantsShadow("scena_sc5")) scena_sc5::SelfTest();
}
