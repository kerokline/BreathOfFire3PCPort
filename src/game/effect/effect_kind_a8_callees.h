// The callees constants this file's functions name as at::k...: the union of their
// groups' namespaces (tools/move_funcs.py, docs/source-layout-round.md 2). The constants
// stay in the groups' headers; a name two of them define differently is refused.
#pragma once

#include "game/effect_4e_callees.h"
#include "game/effect_4f_callees.h"
#include "game/rest_3f_callees.h"

namespace effect_kind_a8::at {
using namespace ::effect_4e::at;
using namespace ::effect_4f::at;
using namespace ::rest_3f::at;
}  // namespace effect_kind_a8::at
