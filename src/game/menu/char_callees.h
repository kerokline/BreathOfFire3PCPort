// The callees constants this file's functions name as at::k...: the union of their
// groups' namespaces (tools/move_funcs.py, docs/source-layout-round.md 2). The constants
// stay in the groups' headers; a name two of them define differently is refused.
#pragma once

#include "game/game_last_callees.h"
#include "game/scena_se_callees.h"
#include "game/scena_sx_callees.h"
#include "game/scena_sx2_callees.h"

namespace char_::at {
using namespace ::game_last::at;
using namespace ::scena_se::at;
using namespace ::scena_sx2::at;
using namespace ::scena_sx::at;
}  // namespace char_::at
