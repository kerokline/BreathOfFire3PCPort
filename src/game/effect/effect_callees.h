// The callees constants this file's functions name as at::k...: the union of their
// groups' namespaces (tools/move_funcs.py, docs/source-layout-round.md 2). The constants
// stay in the groups' headers; a name two of them define differently is refused.
#pragma once

#include "game/battle_e5_callees.h"
#include "game/rest_3b_callees.h"
#include "game/rest_3c_callees.h"
#include "game/rest_3d_callees.h"

namespace effect::at {
using namespace ::battle_e5::at;
using namespace ::rest_3b::at;
using namespace ::rest_3c::at;
using namespace ::rest_3d::at;
}  // namespace effect::at
