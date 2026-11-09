// The callees constants this file's functions name as at::k...: the union of their
// groups' namespaces (tools/move_funcs.py, docs/source-layout-round.md 2). The constants
// stay in the groups' headers; a name two of them define differently is refused.
#pragma once

#include "game/effect_1e_callees.h"
#include "game/effect_1f_callees.h"
#include "game/field_e1_callees.h"

namespace leader_panel::at {
using namespace ::effect_1e::at;
using namespace ::effect_1f::at;
using namespace ::field_e1::at;
}  // namespace leader_panel::at
