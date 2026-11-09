// The callees constants this file's functions name as at::k...: the union of their
// groups' namespaces (tools/move_funcs.py, docs/source-layout-round.md 2). The constants
// stay in the groups' headers; a name two of them define differently is refused.
#pragma once

#include "game/scena_sc9a_callees.h"
#include "game/scena_sc9b_callees.h"

namespace scena_09_2::at {
using namespace ::scena_sc9a::at;
using namespace ::scena_sc9b::at;
}  // namespace scena_09_2::at
