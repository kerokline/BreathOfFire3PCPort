// The callees constants this file's functions name as at::k...: the union of their
// groups' namespaces (tools/move_funcs.py, docs/source-layout-round.md 2). The constants
// stay in the groups' headers; a name two of them define differently is refused.
#pragma once

#include "game/scena_calls_callees.h"
#include "game/scena_se_callees.h"

namespace scena_08::at {
using namespace ::scena_calls::at;
using namespace ::scena_se::at;
}  // namespace scena_08::at
