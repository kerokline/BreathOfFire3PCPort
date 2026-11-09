// The callees constants this file's functions name as at::k...: the union of their
// groups' namespaces (tools/move_funcs.py, docs/source-layout-round.md 2). The constants
// stay in the groups' headers; a name two of them define differently is refused.
#pragma once

#include "game/rest_2c_callees.h"
#include "game/rest_2d_callees.h"

namespace master_talk_2::at {
using namespace ::rest_2c::at;
using namespace ::rest_2d::at;
}  // namespace master_talk_2::at
