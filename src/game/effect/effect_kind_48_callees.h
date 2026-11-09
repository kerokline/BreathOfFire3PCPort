// The callees constants this file's functions name as at::k...: the union of their
// groups' namespaces (tools/move_funcs.py, docs/source-layout-round.md 2). The constants
// stay in the groups' headers; a name two of them define differently is refused.
#pragma once

#include "game/effect_2d_callees.h"
#include "game/effect_2e_callees.h"
#include "game/rest_3e_callees.h"

namespace effect_kind_48::at {
using namespace ::effect_2d::at;
using namespace ::effect_2e::at;
using namespace ::rest_3e::at;
}  // namespace effect_kind_48::at
