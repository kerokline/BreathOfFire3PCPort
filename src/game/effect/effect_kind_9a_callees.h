// The callees constants this file's functions name as at::k...: the union of their
// groups' namespaces (tools/move_funcs.py, docs/source-layout-round.md 2). The constants
// stay in the groups' headers; a name two of them define differently is refused.
#pragma once

#include "game/effect_4d_callees.h"
#include "game/effect_4e_callees.h"

namespace effect_kind_9a::at {
using namespace ::effect_4d::at;
using namespace ::effect_4e::at;
}  // namespace effect_kind_9a::at
