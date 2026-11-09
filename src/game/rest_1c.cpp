// The party sets' field actions of sets 6, 7, 8 and part of 9 - round
// fourteen, wave one, group R1C: the 51 functions 0x51F210..0x520E08 of the cut
// (analysis/round14_cut.tsv; 50 rows and 0x51FA30, code in the band no list
// had). Each read with capstone to its last instruction (docs/rest_1c.md
// section 1). The band repeats a handful of shapes per party set, so many are
// one code with different constants or callees, compared instruction for
// instruction (the doc's section 2):
//
//   the dispatchers (28)        a jmp through a .data table by u16 +0x2C, +2 or +3
//   PartyAction6_Form0Begin     0x51F210 - PartyAction5_Form0Begin's code (field_hidden.cpp)
//   ..._Form0Resolve (3)        0x51F3D0 0x51FF20 0x5205E0 - PartyAction5_Form0Resolve's,
//                               each calling its own set's cell pickup
//   ..._CellPickup (3)          0x51F4B0 (Field_CellPickup's code), 0x520000 0x5206C0
//                               (the zenny's tenfold a twentyfold)
//   ..._Form2Begin (2)          0x51FD40 0x520400 - Form0Begin with the steep test
//                               on the ground's rise, not the slope
//   ..._Form0Begin (3)          0x51FAF0 0x5201A0 0x5208D0 - the jump at a kind-0x30 object
//   ..._Form2Begin / Form1Begin 0x51F670 0x520AA0 - the turn from a blocked way
//   ..._Form2Resolve / ...      0x51F700 0x520B30 - each calling its own cell hit
//   ..._CellHit (2)             0x51F880 0x520C80
//   shared                      0x51F850 0x51FA30 0x51FC80 0x520350 0x520840
//
// Every call out goes through the scenario harness (SH_CALL), so the start-up
// fuzz can stand recorders in for ours as for the originals' copies. No
// divergence: each is a faithful replacement. Where the original would write
// through an index past its table (an effect object's or a sprite object's),
// or jump through a dispatch table to what is not code, ours aborts with a
// message (the round-nine rule); no ordinary caller hands such an index
// (section 5 of the doc). The one unchecked read ordinary play reaches
// (PartyAction_WaitEffectEnd's effect index) is reproduced in place.
#include "game/rest_1c.h"

#include <cstdint>

#include "bof3/symbols.gen.h"
#include "game/move_script_bytes.h"
#include "game/record_views.h"
#include "game/records.h"
#include "game/scenario_harness.h"
#include "hook/detour.h"
#include "hook/log.h"

namespace {

using U = std::uint32_t;

constexpr U kEffectStride = 0x80;

}  // namespace

// ===========================================================================
// The dispatchers: Field_FormActions[6..8] and Field_ActionBySet[6..8] by the
// form, the u16 +0x2C; a form's state table by +2; a state's step table by +3.
// Each a jmp through its .data table, the index unchecked (docs section 3).
// ===========================================================================

// ===========================================================================
// The state handlers
// ===========================================================================

void Rest1C_Inject() {
    if (bof3::WantsShadow("rest_1c")) rest_1c::SelfTest();
}
