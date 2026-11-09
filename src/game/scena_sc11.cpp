// Scenario chapter 11 (the PSX's SCENA11.EMI), compiled into the exe at
// 0x55C040..0x55E4D2. docs/scena_sc11.md.
//
//   - Slot 0, Scena11_Frame, jumps through Scena11_States on the s8 0x8034E2:
//     0 Scena11_Start (the pass flags on, state 1), 1 Scena11_EnterArea (the
//     set-up of the area just entered, then state 2), 2 Scena11_Run, which
//     jumps through Scena11_Runs on MoveScript_Var7 to one of eight scenes.
//   - Each scene is a step machine on the u8 0x8034E5 (a switch in .text,
//     bounded): it waits on the script counter bytes 0x903848.., the message
//     request, the wait word or a stream, then calls two or three engine
//     functions and sets the next step; its last step clears the counters,
//     the step and MoveScript_Var7 (back to "no scene").
//   - Slot 1, Scena11_ObjectTrigger, calls Scena11_Triggers[object +0x86];
//     each of the fourteen handlers starts a scene at a step.
//   - Slot 3, Scena11_ArriveHook (x, z), answers in al; slot 4 answers 0xFF;
//     slot 2 is the shared Scenario_NoHook.
//
// Every call goes through the harness (SH_CALL / SH_AT), so the start-up fuzz
// can stand recorders in for ours as for the originals' copies. The .data
// tables (Scena11_States, Scena11_Runs, Scena11_Triggers) are read in place
// and unchecked, as Scena16_Frame / Scena16_Run read theirs: Scena11_Runs has
// ten entries and MoveScript_Var7 values of 10 and more (three triggers set
// 10, 11 and 14) run on into Scena11_Triggers (docs/scena_sc11.md section 6).
// No divergence: each function is a faithful replacement.
#include "game/scena_sc11.h"

#include <cstdint>

#include "bof3/symbols.gen.h"
#include "game/move_script_bytes.h"
#include "game/record_views.h"
#include "game/records.h"
#include "game/scena_sc11_callees.h"
#include "game/scenario_harness.h"
#include "hook/detour.h"
#include "hook/log.h"

namespace {

using namespace scena_sc11;
using move_script::SetWord;
using move_script::Word;

// The record views (game/record_views.h): rv::Current<T>() reads
// Sprite_Current afresh at each use, as the original does.
// rv::As<records::SpriteObject>(p) views a
// sprite-family pointer; it is used below +0x80 only (the sprite head every
// family shares) unless the pointer is a Sprite_Objects / Sprite_ObjectsExtra /
// Sprite_Kind2 record. rv::As<records::EffectObject>(p) is given effect
// objects (Effect_Objects).

}  // namespace

// ===========================================================================
// Slot 0 and its states

// ===========================================================================
// The scenes (each a switch on the step 0x8034E5)

// ===========================================================================
// Slot 1, the object trigger, and its handlers

// ===========================================================================
// Slots 3 and 4

void ScenaSc11_Inject() {
    if (bof3::WantsShadow("scena_sc11")) scena_sc11::SelfTest();
}
