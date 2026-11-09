// The callees constants this file's functions name as at::k...: the union of their
// groups' namespaces (tools/move_funcs.py, docs/source-layout-round.md 2). The constants
// stay in the groups' headers; a name two of them define differently is refused.
#pragma once

#include "game/boss_sh_callees.h"
#include "game/boss_si_callees.h"

namespace boss_elder::at {
using namespace ::boss_sh::at;
using namespace ::boss_si::at;
}  // namespace boss_elder::at
