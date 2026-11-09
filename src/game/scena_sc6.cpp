// Chapter 6's bank of scenario code, 0x54A910..0x54F080 (round ten group
// SC6). docs/scena_sc6.md.
//
//   - The vtable Scena06_Hooks 0x6610E0 (0x662C80's entry 6): slot 0
//     Scena06_Frame, a tail jump through Scena06_States on the state byte;
//     slot 1 Scena06_ObjectTrigger, a call through Scena06_Objects on the
//     object's +0x86; slot 2 Scena06_StepHook (x, z) answering in al; slot 3
//     the shared Scenario_NoHook; slot 4 Scena06_CellHook (x, z), the record
//     search 0x56D800 over Scena06_Cells and a tail jump through
//     Scena06_CellHandlers.
//   - State 0 Scena06_Start (character record 7 from Scena06_GuestStats),
//     state 1 Scena06_EnterArea (every way out stores state 2), state 2
//     Scena06_Run, a tail jump through Scena06_Runs on MoveScript_Var7; the
//     runs 1..17 are each a switch on the step byte 0x8034E5 whose cases wait
//     on a counter, the request byte or the wait word, do one thing and set
//     the next step.
//   - Scena06_Leap and its three phases (Scena06_LeapPhases): a jump arc of
//     Sprite_Current, called only by area 77's handler 0x40F090.
//
// Every call goes through the harness (SH_CALL / SH_AT), so the start-up fuzz
// can stand recorders in for ours as for the originals' copies. The tables
// are read in place and their entries called directly; the fuzz swaps them
// for recorders or typed stand-ins. No divergence: each function is a
// faithful replacement, except that a dispatcher whose index lies outside its
// table (a negative state or run, or one reading the next table) and a divide
// by a zero count abort where the original would jump through its neighbour
// or fault - the project's rule (round9 doc section 6).
#include "game/scena_sc6.h"

#include <cstdint>
#include <cstring>

#include "bof3/symbols.gen.h"
#include "game/record_views.h"
#include "game/records.h"
#include "game/scena_sc6_callees.h"
#include "game/scenario_harness.h"
#include "hook/detour.h"
#include "hook/log.h"

namespace {

namespace at = scena_sc6::at;

}  // namespace

// ===========================================================================
// Slot 0 and the states

// ===========================================================================
// The runs (Scena06_Runs 1..17), each a switch on the step 0x8034E5; a step
// the switch does not hold does nothing.

// ===========================================================================
// Slot 1, the object trigger, and Scena06_Objects' handlers

// ===========================================================================
// Scena06_Leap: a jump arc of Sprite_Current, stepped once a frame by area
// 77's handler 0x40F090 (the one caller: E8 at 0x40F0E6) with (the
// movement-script object, dx and dz as s8, the lift and the gravity as s16,
// an animation (0xFF none) and a flag byte). Its phase is Sprite_Current +4.

// ===========================================================================
// Slots 2 and 4

// ===========================================================================

void ScenaSc6_Inject() {
    if (bof3::WantsShadow("scena_sc6")) scena_sc6::SelfTest();
}
