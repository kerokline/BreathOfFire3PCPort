// The callees constants this file's functions name as at::k...: the union of their
// groups' namespaces (tools/move_funcs.py, docs/source-layout-round.md 2). The constants
// stay in the groups' headers; a name two of them define alike (one type, one value) is
// named once by a using-declaration, one they define differently is an ambiguity the
// build reports.
#pragma once

#include "game/effect_5a_callees.h"
#include "game/magic_harness.h"

namespace magic_080::at {
using namespace ::effect_5a::at;
using namespace ::magic_harness::at;
}  // namespace magic_080::at
