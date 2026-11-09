// The callees constants this file's functions name as at::k...: the union of their
// groups' namespaces (tools/move_funcs.py, docs/source-layout-round.md 2). The constants
// stay in the groups' headers; a name two of them define differently is refused.
#pragma once

#include "game/effect_5c_callees.h"
#include "game/effect_5d_callees.h"

namespace effect_kind_18_sub_17::at {
using namespace ::effect_5c::at;
using namespace ::effect_5d::at;
}  // namespace effect_kind_18_sub_17::at
