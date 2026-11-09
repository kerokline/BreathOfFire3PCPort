// The callees constants this file's functions name as at::k...: the union of their
// groups' namespaces (tools/move_funcs.py, docs/source-layout-round.md 2). The constants
// stay in the groups' headers; a name two of them define differently is refused.
#pragma once

#include "game/boss_sd_callees.h"
#include "game/boss_se_callees.h"

namespace boss_23::at {
using namespace ::boss_sd::at;
using namespace ::boss_se::at;
}  // namespace boss_23::at
