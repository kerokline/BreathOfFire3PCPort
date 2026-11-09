// The callees constants this file's functions name as at::k...: the union of their
// groups' namespaces (tools/move_funcs.py, docs/source-layout-round.md 2). The constants
// stay in the groups' headers; a name two of them define differently is refused.
#pragma once

#include "game/effect_1c_callees.h"
#include "game/field_c1_callees.h"
#include "game/field_c2_callees.h"
#include "game/rest_3e_callees.h"

namespace effect_kind_34::at {
using namespace ::effect_1c::at;
using namespace ::field_c1::at;
using namespace ::field_c2::at;
using namespace ::rest_3e::at;
}  // namespace effect_kind_34::at
