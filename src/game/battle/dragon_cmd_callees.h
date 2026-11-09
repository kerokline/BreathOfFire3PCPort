// The callees constants this file's functions name as at::k...: the union of their
// groups' namespaces (tools/move_funcs.py, docs/source-layout-round.md 2). The constants
// stay in the groups' headers; a name two of them define differently is refused.
#pragma once

#include "game/battle_e5_callees.h"
#include "game/battle_e6_callees.h"
#include "game/magic_harness.h"
#include "game/rest_3d_callees.h"
#include "game/rest_4a_callees.h"

namespace dragon_cmd::at {
using namespace ::battle_e5::at;
using namespace ::battle_e6::at;
using namespace ::magic_harness::at;
using namespace ::rest_3d::at;
using namespace ::rest_4a::at;
}  // namespace dragon_cmd::at
