// The callees constants this file's functions name as at::k...: the union of their
// groups' namespaces (tools/move_funcs.py, docs/source-layout-round.md 2). The constants
// stay in the groups' headers; a name two of them define differently is refused.
#pragma once

#include "game/effect_2e_callees.h"
#include "game/effect_2f_callees.h"

namespace effect_kind_4e::at {
using namespace ::effect_2e::at;
using namespace ::effect_2f::at;
}  // namespace effect_kind_4e::at
