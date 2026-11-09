// Round fourteen group R1E (docs/rest_1e.md): the 47 functions of
// analysis/round14_cut.tsv's group R1E, 0x5226D0..0x523EC2, each read with
// capstone to its last instruction (2026-10-04). The field actions of party
// sets 13, 14 and 15 and of set 16's forms 0 and 1: Field_ActionBySet /
// Field_FormActions (by the party set) reach a set's dispatcher, which jumps
// through the set's forms table by the sprite's u16 +0x2C; a form's dispatcher
// jumps through its states table by +2 (two forms: through a modes table by +2,
// then a mode's states by +3); the states below. The pattern repeats per set:
// the same code at two or three addresses (capstone, the constants equal), so
// one body serves each shape and the copies differ only in which of the
// group's cell helpers they call.
//
// Every call goes through the scenario harness (SH_CALL), so the start-up fuzz
// can stand recorders in for ours as for the originals' copies. Sprite_Current
// is read again wherever the original reads [0x937F88] again after a call. No
// divergence: each is a faithful replacement. Where the original jumps through
// a table past its end, or indexes Effect_Objects or the sprite records past
// them by a byte it was handed, ours aborts with a message (docs/rest_1e.md
// section 5).
#include "game/rest_1e.h"

#include <cstdint>

#include "bof3/symbols.gen.h"
#include "game/move_script_bytes.h"
#include "game/record_views.h"
#include "game/records.h"
#include "game/rest_0a.h"
#include "game/rest_1e_callees.h"
#include "game/scenario_harness.h"
#include "hook/detour.h"
#include "hook/log.h"

namespace {

namespace at = rest_1e::at;

// --- the shapes' bodies -------------------------------------------------------------

}  // namespace

// ===========================================================================
// Shared by many sets
// ===========================================================================

// ===========================================================================
// The dispatchers (0x12 / 0x13 bytes each): mov ecx, [Sprite_Current]; xor
// eax, eax; mov al, [ecx + 2 or 3] / mov ax, [ecx + 0x2C]; jmp [table + eax *
// 4]. The table's length is the run of code pointers to the next table (no
// reader bounds it; docs/rest_1e.md section 3).
// ===========================================================================

// ===========================================================================
// The states (each the shape named in its body's comment)
// ===========================================================================

void Rest1E_Inject() {
    if (bof3::WantsShadow("rest_1e")) rest_1e::SelfTest();
}
