// The callees constants this file's functions name as at::k...: the union of their
// groups' namespaces (tools/move_funcs.py, docs/source-layout-round.md 2). The constants
// stay in the groups' headers; a name two of them define differently is refused.
#pragma once

#include "game/member_sprites_callees.h"
#include "game/rest_1a_callees.h"

namespace member::at {
using namespace ::member_sprites::at;
using namespace ::rest_1a::at;
}  // namespace member::at
