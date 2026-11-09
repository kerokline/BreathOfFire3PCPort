// The callees constants this file's functions name as at::k...: the union of their
// groups' namespaces (tools/move_funcs.py, docs/source-layout-round.md 2). The constants
// stay in the groups' headers; a name two of them define differently is refused.
#pragma once

#include "game/field_c1_callees.h"
#include "game/rest_4f_callees.h"

namespace title::at {
using namespace ::field_c1::at;
using namespace ::rest_4f::at;
}  // namespace title::at
