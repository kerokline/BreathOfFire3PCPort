// The field core's second half - round twelve, group FC2, the band
// 0x46BBF0..0x46D5ED of the cut (analysis/round12_cut.tsv), taken with the
// scenario harness in field mode (scenario_harness.h, docs/scenario_harness.md
// section 7). Four kinds of the Effect_Objects pool (Effect_RunObjects makes a
// record Sprite_Current and calls Effect_KindHandlers[+5]):
//
//   kind 0x30  states 1..3 of FC1's 0x46BB30 and their helpers: slide a cell
//              in the leader's facing, claiming the cells it rests on; when
//              the way is blocked, shatter - 24 faces of its model tumble and
//              eight sparks fly (0x92BF80 / 0x92C4C0);
//   kind 0x34  EffectKind34_Run by +1 over five variants (and FC1's
//              0x46A310), each a small state table by +2: bursts, debris,
//              falling pieces;
//   kind 0x3A  EffectKind3A_Run by +1: thrown ahead of the leader, it clears
//              the 0xFD cells it hits (FE2's 0x5728D0) and now and then finds
//              zenny (FE1's 0x5307C0);
//   kind 0x41  EffectKind41_Run by +1: the number +6 drawn over party member
//              +0xB, bouncing and blinking out.
//
// Every call goes through the harness (SH_CALL / SH_AT), so the start-up fuzz
// can stand recorders in for ours as for the originals' copies. No divergence:
// each is a faithful replacement. Where the original jumps through a state
// table past its end, or writes a record past its pool on an answer the callee
// never gives, ours aborts with a message (docs/field_c2.md section 2).
#include "game/field_c2.h"

#include <cstdint>
#include <cstring>

#include "bof3/symbols.gen.h"
#include "game/field_c2_callees.h"
#include "game/move_script_bytes.h"
#include "game/record_views.h"
#include "game/records.h"
#include "game/scenario_harness.h"
#include "hook/detour.h"
#include "hook/log.h"

namespace {

namespace at = field_c2::at;
using U = std::uint32_t;

U Key(const void* p) { return Low32(p); }

// A PSX MATRIX as the originals lay it out on their stack.
struct Matrix {
    short m[9];
    short pad;
    std::int32_t t[3];
};
static_assert(sizeof(Matrix) == 0x20, "a MATRIX is 0x20 bytes");

}  // namespace

// ===========================================================================
// Kind 0x30: FC1's EffectKind30 dispatcher 0x46BB30 by +1 through 0x653FE4
// (0x46BB50 FC1's, then these three)
// ===========================================================================

// ===========================================================================
// Kind 0x34: Effect_KindHandlers[0x34]; +1 the variant, +2 its state
// ===========================================================================

// ===========================================================================
// Kind 0x3A: Effect_KindHandlers[0x3A]
// ===========================================================================

// ===========================================================================
// Kind 0x41: Effect_KindHandlers[0x41]
// ===========================================================================

void FieldC2_Inject() {
    if (bof3::WantsShadow("field_c2")) field_c2::SelfTest();
}
