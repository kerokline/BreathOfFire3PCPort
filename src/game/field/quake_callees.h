// The callees constants this file's functions name as at::k...: the union of their
// groups' namespaces (tools/move_funcs.py, docs/source-layout-round.md 2). The constants
// stay in the groups' headers; a name two of them define differently is refused.
#pragma once

#include "game/battle_e6_callees.h"
#include "game/magic_s23_callees.h"
#include "game/rest_3g_callees.h"

namespace quake::at {
using namespace ::battle_e6::at;
using namespace ::magic_s23::at;
using namespace ::rest_3g::at;
}  // namespace quake::at
