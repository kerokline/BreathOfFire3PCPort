// The callees constants this file's functions name as at::k...: the union of their
// groups' namespaces (tools/move_funcs.py, docs/source-layout-round.md 2). The constants
// stay in the groups' headers; a name two of them define differently is refused.
#pragma once

#include "game/field_o_callees.h"
#include "game/scena_sx_callees.h"
#include "game/scena_sx2_callees.h"

namespace event_cond::at {
using namespace ::field_o::at;
using namespace ::scena_sx2::at;
using namespace ::scena_sx::at;
}  // namespace event_cond::at
