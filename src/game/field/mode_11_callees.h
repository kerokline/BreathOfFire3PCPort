// The callees constants this file's functions name as at::k...: the union of their
// groups' namespaces (tools/move_funcs.py, docs/source-layout-round.md 2). The constants
// stay in the groups' headers; a name two of them define differently is refused.
#pragma once

#include "game/field_e2_callees.h"
#include "game/field_event_callees.h"
#include "game/rest_2a_callees.h"
#include "game/scena_sx_callees.h"

namespace mode_11::at {
using namespace ::field_e2::at;
using namespace ::field_event::at;
using namespace ::rest_2a::at;
using namespace ::scena_sx::at;
}  // namespace mode_11::at
