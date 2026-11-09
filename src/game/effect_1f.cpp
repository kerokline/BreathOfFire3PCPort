// Round thirteen group E1F: the cut's fifteen rows at 0x52A6C0..0x52D07C
// (analysis/round13_cut.tsv), each read with capstone to its last instruction
// (docs/effect_1f.md section 1). What they are, by the code:
//
//   - a field menu whose state lives in Effect_Objects record 6 (0x7E14E0):
//     the dispatch on its three-way choice (ChoiceMenu_Run), the extra-slot
//     sub-menu's step dispatch and its three steps (ExtraSlots_*);
//   - a sprite pass game mode 8 runs every frame (Sprite_UpdateAllScaled and
//     the per-record Sprite_UpdateScreenScaled, which E1A's states call too);
//   - the effect records' reset E1E's menu calls (Effect_ResetFirstSeven);
//   - the kind points FE1's panel prints (FieldPanel_KindPoints / _KindTotal);
//   - game mode 8's three steps (GameMode8_*): not effect code at all;
//   - the two primitive helpers 130 effect and panel sites call
//     (UiSprite_SetMode, UiSprite_Draw).
//
// Every call out goes through the scenario harness (SH_CALL / SH_AT), so the
// start-up fuzz can stand recorders in for ours as for the originals' copies.
// No divergence: each is a faithful replacement. Where the original jumps
// through a state table unchecked, ours aborts past the table; where it divides
// by a value that can be 0 (Sprite_UpdateScreenScaled), ours aborts there. The
// unchecked reads of the image's data tables (UiSprite_Modes, UiSprite_Sheet,
// the kinds' records, NameTable_Accessories, the id list by an s16) read the
// same bytes in place as the original - no fault on either side - and are
// described as latent defects (docs/effect_1f.md section 6), not aborted.
#include "game/effect_1f.h"

#include <cstdint>
#include <cstring>

#include "bof3/symbols.gen.h"
#include "game/record_views.h"
#include "game/effect_1f_callees.h"
#include "game/scenario_harness.h"
#include "hook/detour.h"
#include "hook/log.h"
#include "input/sigma.h"

namespace {

namespace at = effect_1f::at;

}  // namespace

void Effect1F_Inject() {
    if (bof3::WantsShadow("effect_1f")) effect_1f::SelfTest();
}
