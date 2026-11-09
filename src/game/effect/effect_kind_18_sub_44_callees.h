// The callees constants this file's functions name as at::k...: the union of their
// groups' namespaces (tools/move_funcs.py, docs/source-layout-round.md 2). The constants
// stay in the groups' headers; a name two of them define differently is refused.
#pragma once

#include "game/effect_6b_callees.h"
#include "game/effect_6c_callees.h"
#include "game/rest_3g_callees.h"

namespace effect_kind_18_sub_44::at {
using namespace ::effect_6b::at;
using namespace ::effect_6c::at;
using namespace ::rest_3g::at;
}  // namespace effect_kind_18_sub_44::at
