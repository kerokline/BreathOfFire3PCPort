// The callees constants this file's functions name as at::k...: the union of their
// groups' namespaces (tools/move_funcs.py, docs/source-layout-round.md 2). The constants
// stay in the groups' headers; a name two of them define differently is refused.
#pragma once

#include "game/battle_e7_callees.h"
#include "game/rest_2g_callees.h"
#include "game/rest_2h_callees.h"

namespace menu_list::at {
using namespace ::battle_e7::at;
using namespace ::rest_2g::at;
using namespace ::rest_2h::at;
}  // namespace menu_list::at
