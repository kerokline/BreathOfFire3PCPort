// The callees constants this file's functions name as at::k...: the union of their
// groups' namespaces (tools/move_funcs.py, docs/source-layout-round.md 2). The constants
// stay in the groups' headers; a name two of them define differently is refused.
#pragma once

#include "game/effect_4a_callees.h"
#include "game/effect_4b_callees.h"

namespace effect_kind_87::at {
using namespace ::effect_4a::at;
using namespace ::effect_4b::at;
}  // namespace effect_kind_87::at
