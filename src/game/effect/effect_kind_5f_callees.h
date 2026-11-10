// The callees constants this file's functions name as at::k...: the union of their
// groups' namespaces (tools/move_funcs.py, docs/source-layout-round.md 2). The constants
// stay in the groups' headers; a name two of them define differently is refused.
#pragma once

#include "game/effect_2g_callees.h"
#include "game/effect_3a_callees.h"
#include "game/magic_harness.h"
#include "game/rest_3e_callees.h"
#include "game/rest_3f_callees.h"

namespace effect_kind_5f::at {
using namespace ::effect_2g::at;
using namespace ::effect_3a::at;
using namespace ::magic_harness::at;
using namespace ::rest_3e::at;
using namespace ::rest_3f::at;
}  // namespace effect_kind_5f::at
