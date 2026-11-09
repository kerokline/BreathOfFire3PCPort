// The callees constants this file's functions name as at::k...: the union of their
// groups' namespaces (tools/move_funcs.py, docs/source-layout-round.md 2). The constants
// stay in the groups' headers; a name two of them define differently is refused.
#pragma once

#include "game/field_e1_callees.h"
#include "game/field_event_callees.h"

namespace field_4::at {
using namespace ::field_e1::at;
using namespace ::field_event::at;
}  // namespace field_4::at
