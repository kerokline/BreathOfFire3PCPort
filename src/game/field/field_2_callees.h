// The callees constants this file's functions name as at::k...: the union of their
// groups' namespaces (tools/move_funcs.py, docs/source-layout-round.md 2). The constants
// stay in the groups' headers; a name two of them define differently is refused.
#pragma once

#include "game/battle_e6_callees.h"
#include "game/field_event_callees.h"
#include "game/rest_4a_callees.h"

namespace field_2::at {
using namespace ::battle_e6::at;
using namespace ::field_event::at;
using namespace ::rest_4a::at;
}  // namespace field_2::at
