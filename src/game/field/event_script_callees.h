// The callees constants this file's functions name as at::k...: the union of their
// groups' namespaces (tools/move_funcs.py, docs/source-layout-round.md 2). The constants
// stay in the groups' headers; a name two of them define differently is refused.
#pragma once

#include "game/field_o_callees.h"
#include "game/scena_se_callees.h"

namespace event_script::at {
using namespace ::field_o::at;
using namespace ::scena_se::at;
}  // namespace event_script::at
