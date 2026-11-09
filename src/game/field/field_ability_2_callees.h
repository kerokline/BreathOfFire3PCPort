// The callees constants this file's functions name as at::k...: the union of their
// groups' namespaces (tools/move_funcs.py, docs/source-layout-round.md 2). The constants
// stay in the groups' headers; a name two of them define differently is refused.
#pragma once

#include "game/field_s_callees.h"
#include "game/rest_2d_callees.h"
#include "game/rest_2e_callees.h"

namespace field_ability_2::at {
using namespace ::field_s::at;
using namespace ::rest_2d::at;
using namespace ::rest_2e::at;
}  // namespace field_ability_2::at
