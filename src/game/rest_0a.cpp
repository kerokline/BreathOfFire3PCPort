// The party sets' field actions' shared helpers - round fourteen, stage-A
// group R0A, the seven functions 0x51C390..0x524E4E of the cut
// (analysis/round14_cut.tsv), taken before wave one so that its six
// field-action groups call them by name. Each read with capstone to its last
// instruction (docs/rest_0a.md section 1); each is a helper called directly
// (E8) by the party sets' state handlers, none a state, a case or a shared tail.
//
// Every call out goes through the scenario harness (SH_CALL), so the start-up
// fuzz can stand recorders in for ours as for the originals' copies. No
// divergence: each is a faithful replacement. Where the original would index
// Effect_Objects past its 20 records by an argument (0x51C6A0, 0x51DD70), ours
// aborts with a message; no caller passes such an index (section 5).
#include "game/rest_0a.h"

#include <cstdint>

#include "bof3/symbols.gen.h"
#include "game/move_script_bytes.h"
#include "game/record_views.h"
#include "game/records.h"
#include "game/scenario_harness.h"
#include "hook/detour.h"
#include "hook/log.h"

namespace {

using move_script::SetLong;
using move_script::SetWord;
using move_script::Word;

// ObjTrio's record 1 (0x802D40 + 0x14C): the originals start their member loop
// here, at index 1, as `mov edi, 0x802E8C`.
// kMemberOne: now Field_Members[4] (symbols.toml). ObjTrio + 0x14C
// kSteps: now Field_DirectionSteps (symbols.toml). Field_DirectionSteps
// kEffects: now Effect_Objects (symbols.toml). Effect_Objects

}  // namespace

void Rest0A_Inject() {
    if (bof3::WantsShadow("rest_0a")) rest_0a::SelfTest();
}
