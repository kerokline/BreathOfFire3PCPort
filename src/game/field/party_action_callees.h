// The callees constants this file's functions name as at::k...: the union of their
// groups' namespaces (tools/move_funcs.py, docs/source-layout-round.md 2). The constants
// stay in the groups' headers; a name two of them define differently is refused.
#pragma once

#include "game/rest_1a_callees.h"
#include "game/rest_1d_callees.h"
#include "game/rest_1e_callees.h"
#include "game/scena_se_callees.h"

namespace party_action::at {
using namespace ::rest_1a::at;
using namespace ::rest_1d::at;
using namespace ::rest_1e::at;
using namespace ::scena_se::at;
}  // namespace party_action::at
