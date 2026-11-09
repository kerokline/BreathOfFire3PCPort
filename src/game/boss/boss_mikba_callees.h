// The callees constants this file's functions name as at::k...: the union of their
// groups' namespaces (tools/move_funcs.py, docs/source-layout-round.md 2). The constants
// stay in the groups' headers; a name two of them define differently is refused.
#pragma once

#include "game/boss_sg_callees.h"
#include "game/boss_sh_callees.h"

namespace boss_mikba::at {
using namespace ::boss_sg::at;
using namespace ::boss_sh::at;
}  // namespace boss_mikba::at
