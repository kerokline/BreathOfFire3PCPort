// The callees constants this file's functions name as at::k...: the union of their
// groups' namespaces (tools/move_funcs.py, docs/source-layout-round.md 2). The constants
// stay in the groups' headers; a name two of them define differently is refused.
#pragma once

#include "game/effect_2b_callees.h"
#include "game/effect_2c_callees.h"

namespace effect_kind_3d::at {
using namespace ::effect_2b::at;
using namespace ::effect_2c::at;
}  // namespace effect_kind_3d::at
