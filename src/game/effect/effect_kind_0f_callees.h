// The callees constants this file's functions name as at::k...: the union of their
// groups' namespaces (tools/move_funcs.py, docs/source-layout-round.md 2). The constants
// stay in the groups' headers; a name two of them define differently is refused.
#pragma once

#include "game/effect_1a_callees.h"
#include "game/effect_1b_callees.h"
#include "game/rest_4f_callees.h"

namespace effect_kind_0f::at {
using namespace ::effect_1a::at;
using namespace ::effect_1b::at;
using namespace ::rest_4f::at;
}  // namespace effect_kind_0f::at
