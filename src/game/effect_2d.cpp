// Effect kinds 0x45..0x49 - round thirteen, wave two, group E2D: the 54
// functions of the cut (analysis/round13_cut.tsv) in 0x4771B0..0x4789C1, each
// read with capstone to its last instruction (docs/effect_2d.md section 1).
// Effect_RunObjects (ours) makes each live record of Effect_Objects (20 of 0x80
// bytes) Sprite_Current and calls Effect_KindHandlers[+5]; each kind here is a
// dispatcher by +1 through its state table (none bounded by a compare) and the
// states it names:
//
//   kind 0x45  a 72 x 72 panel at (0x82, 0xA) - two filled squares, a border,
//              a grid - and a trace of 36 samples of a sine wave; state 1 slows
//              the wave and grows it while the flag 0x25 of the chapter's row is
//              set, state 2 reads the confirm button and sets or clears the flag
//              0x26 by where the wave stands (chapter 6's run 13 spawns it,
//              scena_sc6.cpp);
//   kind 0x46  a screen-wide tile whose colour steps up and down by 4 over 12
//              frames, five times, then the chapters' counter byte 0x903848 up;
//              every field sprite's screen place redrawn each frame;
//   kind 0x47  two free sprites put beside the leader (or sprite object 2 when
//              +0xB is 0), tinted red and blue, the leader (or object 2) hidden
//              for four frames (area 85's handlers 3 and 4, area_w2b.cpp);
//   kind 0x48  rings of four sparks every four frames, +0xB rings, then the
//              sparks run out (area 52's handlers 6 and 7, area_w1c.cpp);
//   kind 0x49  ten variants picked by +1 at the spawn (chapter 7,
//              Scena07_TakeEffect49): 0 puffs, 1 a CLUT fade, 2 a glow flown to a
//              point, 3 dust, 4 (and 5..9, E2E's) by +2.
//
// Every call goes through the harness (SH_CALL / SH_AT), so the start-up fuzz
// can stand recorders in for ours as for the originals' copies. No divergence:
// each is a faithful replacement. Where the original jumps through a state
// table past its end, writes a sprite record past the pool on an index the
// callee never gives, or would loop forever, ours aborts with a message
// (docs/effect_2d.md section 2).
#include "game/effect_2d.h"

#include <bit>
#include <cstdint>
#include <cstring>

#include "bof3/symbols.gen.h"
#include "game/cheats.h"
#include "game/effect_2d_callees.h"
#include "game/effect_gte.h"
#include "game/move_script_bytes.h"
#include "game/record_views.h"
#include "game/records.h"
#include "game/scenario_harness.h"
#include "game/widescreen.h"
#include "hook/detour.h"
#include "hook/log.h"

namespace {

namespace at = effect_2d::at;

}  // namespace

// ===========================================================================
// Kind 0x45: Effect_KindHandlers[0x45] (0x655464), EffectKind45_States (four)
// ===========================================================================

// ===========================================================================
// Kind 0x46: Effect_KindHandlers[0x46] (0x655468), EffectKind46_States (five)
// ===========================================================================

// ===========================================================================
// Kind 0x47: Effect_KindHandlers[0x47] (0x65546C), EffectKind47_States (three)
// ===========================================================================

// ===========================================================================
// Kind 0x48: Effect_KindHandlers[0x48] (0x655470), EffectKind48_States (three)
// ===========================================================================

// ===========================================================================
// Kind 0x49: Effect_KindHandlers[0x49] (0x655474), EffectKind49_Variants (ten)
// ===========================================================================

void Effect2D_Inject() {
    if (bof3::WantsShadow("effect_2d")) effect_2d::SelfTest();
    // DIV-0188 guards the lighthouse gauge's judgement in EffectKind45_Tune
    // and _Aim; Capcom's would judge as before and the cheat would be silently
    // half off.
    if (Cheats_MinigameNoFail() && (!bof3::IsEnabled(bof3::addr::EffectKind45_Tune) || !bof3::IsEnabled(bof3::addr::EffectKind45_Aim)))
        bof3::Fatal("BOF3X_NO_FAIL=1 needs EffectKind45_Tune and EffectKind45_Aim ours, and one is disabled");
}
