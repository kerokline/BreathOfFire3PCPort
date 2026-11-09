// Party sets 2..6's field actions - round fourteen, wave-one group R1B, the 47
// functions 0x51D710..0x51F202 of the cut (analysis/round14_cut.tsv). Each read
// with capstone to its last instruction (docs/rest_1b.md section 1): 29
// dispatchers through .data tables, 14 states, 4 cell handlers. Seven of the
// states and cell handlers are instruction-for-instruction copies of code
// already ours (PartyAction5_Form0Begin, PartyAction5_Form0Resolve,
// Field_CellPickup in field_hidden.cpp; compared with capstone: only the call
// targets differ), kept as their own functions because the tables and calls
// reach each copy by its own address.
//
// Every call out goes through the scenario harness (SH_CALL), so the start-up
// fuzz can stand recorders in for ours as for the originals' copies; every
// .data table is read in place. No divergence: each is a faithful replacement.
// Where the original would index past a table (an object record by an answer
// no callee gives, a table entry that is not code), ours aborts with a message
// (section 6).
#include "game/rest_1b.h"

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

constexpr unsigned kObjectStride = 0xA4;   // Sprite_Objects and Sprite_ObjectsExtra

}  // namespace

// ===========================================================================
// The dispatchers (section 1.1): `mov ecx, [Sprite_Current]; xor eax, eax;
// mov ax / al, ...; jmp [eax * 4 + table]`, the index unchecked.
// ===========================================================================

#undef R1B_TABLE

// ===========================================================================
// The states and the cell handlers (sections 1.2..1.5)
// ===========================================================================

void Rest1B_Inject() {
    if (bof3::WantsShadow("rest_1b")) rest_1b::SelfTest();
}
