// The effect engine's GTE helpers - round thirteen, stage-A group EGT, the four
// functions 0x494060..0x494270 of the cut (analysis/round13_cut.tsv), taken
// before the round's 35 effect groups so that every one of them calls these by
// name. Each read with capstone to its last instruction (docs/effect_gte.md
// section 1); the labelling tool's "effect kind 186" for them was a hypothesis
// by address, and they are not a kind's code but helpers the kinds call.
//
// Every call out goes through the scenario harness (SH_CALL), so the start-up
// fuzz can stand recorders in for ours as for the originals' copies. Each is a
// faithful replacement but one: EffectGte_SetDiagonalOne writes the
// PlayStation's 0x1000 where Capcom's PC writes 1 (DIV-0126,
// docs/lighthouse-rays.md; BOF3X_ORIGINAL=DiagonalIdentity for the 1s). Where
// the original divides by a depth that can be 0 (EffectGte_ProjectSize), ours
// aborts with a message.
#include "game/effect_gte.h"

#include <cstdint>

#include "bof3/symbols.gen.h"
#include "game/scenario_harness.h"
#include "hook/detour.h"
#include "hook/log.h"

namespace {

// libgte's MATRIX: nine s16 of rotation, two bytes of padding, a translation
// of three s32.
struct Matrix {
    short m[9];
    short pad;
    long t[3];
};
static_assert(sizeof(Matrix) == 0x20, "a MATRIX is 32 bytes");

}  // namespace

namespace effect_gte {
// DIV-0126 (docs/lighthouse-rays.md): 1 through PatchBytes("DiagonalIdentity")
// in EffectGte_Inject, after the fuzz, which runs Capcom's 1s. While 0 the
// diagonal is Capcom's PC value, 1; set, it is the PlayStation twin's 0x1000.
std::uint32_t g_identity = 0;
}  // namespace effect_gte

void EffectGte_Inject() {
    if (bof3::WantsShadow("effect_gte")) effect_gte::SelfTest();
    {
        // DIV-0126: armed after the fuzz above, which sees Capcom's 1s.
        const std::uint8_t was[] = {0, 0, 0, 0}, is[] = {1, 0, 0, 0};
        const bool on = bof3::PatchBytes(
            "DiagonalIdentity", Low32(&effect_gte::g_identity),
            was, is, 4);
        if (on && bof3::WantsShadow("effect_gte")) effect_gte::IdentityCheck();
        bof3::Log("DIV-0126    EffectGte_SetDiagonalOne's diagonal is 0x1000, the PlayStation's identity (on unless the line above says OFF)");
    }
}
