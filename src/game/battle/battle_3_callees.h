// The callees constants this file's functions name as at::k...: the union of their
// groups' namespaces (tools/move_funcs.py, docs/source-layout-round.md 2). The constants
// stay in the groups' headers; a name two of them define differently is refused.
#pragma once

#include "game/battle_e4_callees.h"
#include "game/rest_3b_callees.h"

namespace battle_3::at {
using namespace ::battle_e4::at;
using namespace ::rest_3b::at;
}  // namespace battle_3::at
