// The callees constants this file's functions name as at::k...: the union of their
// groups' namespaces (tools/move_funcs.py, docs/source-layout-round.md 2). The constants
// stay in the groups' headers; a name two of them define differently is refused.
#pragma once

#include "game/rest_4b_callees.h"
#include "game/rest_4c_callees.h"
#include "game/rest_4d_callees.h"

namespace commu_board::at {
using namespace ::rest_4b::at;
using namespace ::rest_4c::at;
using namespace ::rest_4d::at;
}  // namespace commu_board::at
