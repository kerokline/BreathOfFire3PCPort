// Effect kinds 0x48 (states 7..12) and 0x4A..0x4E - round thirteen, wave two,
// group E2E: the 51 functions of the cut (analysis/round13_cut.tsv) in
// 0x4789D0..0x47B7CF, each read with capstone to its last instruction
// (docs/effect_2e.md section 1). Effect_RunObjects (ours) makes each live
// record of Effect_Objects (20 of 0x80 bytes) Sprite_Current and calls
// Effect_KindHandlers[+5]; each dispatcher here jumps through its state table
// by a byte of the record (none bounded by a compare):
//
//   kind 0x48's states 7..12 (E2D's dispatcher 0x4781B0 by +1 through
//   0x654578; kind 0x49's 0x478550 reaches the same cells as its states 4..9)
//   are each a dispatcher by +2 through a table of their own - state 7's is
//   E2D's (0x4789B0), 8..12 ours:
//     7   a ring record at 0x92D1C8 swelling, widening and lifting;
//     8   a spiral record at 0x92C4A4 at the record's point, turning up;
//     9   sparks (the 8 records at 0x92BF80) round the leader;
//     10  the spiral at the leader, turning down;
//     11  a lit sphere at the leader (EffectSphere_Build / _Draw);
//     12  a burst record at 0x92C060 that travels to a point, holds, grows
//         and shrinks;
//   kind 0x4A  a trail of 64 points after Sprite_Objects[1], a glow at its head;
//   kind 0x4B  32 debris scattering at a fixed cell;
//   kind 0x4C  random spark lines at a fixed cell for 300 frames;
//   kind 0x4D  a textured column rising at its point;
//   kind 0x4E  a disc at the leader's screen point (E2F's 0x47B7D0 draws it).
//
// Every call goes through the harness (SH_CALL / SH_AT), so the start-up fuzz
// can stand recorders in for ours as for the originals' copies. No divergence:
// each is a faithful replacement. Where the original jumps through a state
// table past its end or indexes the sphere past its 0x1E2 points, ours aborts
// with a message (docs/effect_2e.md section 2).
#include "game/effect_2e.h"

#include <cstdint>
#include <cstring>

#include "bof3/symbols.gen.h"
#include "game/effect_2e_callees.h"
#include "game/effect_gte.h"
#include "game/move_script_bytes.h"
#include "game/record_views.h"
#include "game/records.h"
#include "game/scenario_harness.h"
#include "hook/detour.h"
#include "hook/log.h"

namespace {

namespace at = effect_2e::at;

// libgte's MATRIX: nine s16 of rotation, two bytes of padding, a translation
// of three s32.
struct Matrix {
    short m[9];
    short pad;
    long t[3];
};
static_assert(sizeof(Matrix) == 0x20, "a MATRIX is 32 bytes");

}  // namespace

// ===========================================================================
// Kind 0x48's state 7: E2D's dispatcher 0x4789B0 by +2 through 0x6545FC (four)
// ===========================================================================

// ===========================================================================
// Kind 0x48's state 8: EffectKind48_State8_Steps (two), by +2
// ===========================================================================

// ===========================================================================
// Kind 0x48's state 9: EffectKind48_State9_Steps (three), by +2
// ===========================================================================

// ===========================================================================
// Kind 0x48's state 10: EffectKind48_State10_Steps (two), by +2
// ===========================================================================

// ===========================================================================
// Kind 0x48's state 11: EffectKind48_State11_Steps (two), by +2
// ===========================================================================

// ===========================================================================
// Kind 0x48's state 12: EffectKind48_State12_Steps (eight: five here, then
// WeretigerFx_Next twice and Effect_StateRelease), by +2
// ===========================================================================

// ===========================================================================
// Helpers: an angle mean, the sphere
// ===========================================================================

// ===========================================================================
// Kind 0x4A: Effect_KindHandlers[0x4A] (0x655478), EffectKind4A_States (two)
// ===========================================================================

// ===========================================================================
// Kind 0x4B: Effect_KindHandlers[0x4B] (0x65547C), EffectKind4B_States (two)
// ===========================================================================

// ===========================================================================
// Kind 0x4C: Effect_KindHandlers[0x4C] (0x655480), EffectKind4C_States (two)
// ===========================================================================

// ===========================================================================
// Kind 0x4D: Effect_KindHandlers[0x4D] (0x655484), EffectKind4D_States (three)
// ===========================================================================

// ===========================================================================
// Kind 0x4E: Effect_KindHandlers[0x4E] (0x655488), EffectKind4E_States (five,
// the last Effect_StateRelease)
// ===========================================================================

void Effect2E_Inject() {
    if (bof3::WantsShadow("effect_2e")) effect_2e::SelfTest();
}
