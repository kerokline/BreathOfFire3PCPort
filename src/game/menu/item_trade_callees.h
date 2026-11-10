// The callees constants this file's functions name as at::k...: the union of their
// groups' namespaces (tools/move_funcs.py, docs/source-layout-round.md 2). The constants
// stay in the groups' headers; a name two of them define differently is refused.
#pragma once

#include "game/effect_1g_callees.h"
#include "game/field_e2_callees.h"
#include "game/game_last_callees.h"

namespace item_trade::at {
using namespace ::effect_1g::at;
using namespace ::field_e2::at;
using namespace ::game_last::at;
}  // namespace item_trade::at
