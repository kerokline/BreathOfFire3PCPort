// The callees constants this file's functions name as at::k...: the union of their
// groups' namespaces (tools/move_funcs.py, docs/source-layout-round.md 2). The constants
// stay in the groups' headers; a name two of them define differently is refused.
#pragma once

#include "game/battle_e1_callees.h"
#include "game/rest_3a_callees.h"

namespace battle_action::at {
using namespace ::battle_e1::at;
using namespace ::rest_3a::at;
}  // namespace battle_action::at
