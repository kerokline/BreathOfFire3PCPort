// The callees constants this file's functions name as at::k...: the union of their
// groups' namespaces (tools/move_funcs.py, docs/source-layout-round.md 2). The constants
// stay in the groups' headers; a name two of them define differently is refused.
#pragma once

#include "game/field_s_callees.h"
#include "game/rest_2c_callees.h"

namespace shared_list::at {
using namespace ::field_s::at;
using namespace ::rest_2c::at;
}  // namespace shared_list::at
