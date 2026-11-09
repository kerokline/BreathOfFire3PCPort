// The callees constants this file's functions name as at::k...: the union of their
// groups' namespaces (tools/move_funcs.py, docs/source-layout-round.md 2). The constants
// stay in the groups' headers; a name two of them define alike (one type, one value) is
// named once by a using-declaration, one they define differently is an ambiguity the
// build reports.
#pragma once

#include "game/area_w0c_callees.h"
#include "game/rest_3a_callees.h"

namespace world_map_33::at {
using namespace ::area_w0c::at;
using namespace ::rest_3a::at;
}  // namespace world_map_33::at
