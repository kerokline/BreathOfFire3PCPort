// The callees constants this file's functions name as at::k...: the union of their
// groups' namespaces (tools/move_funcs.py, docs/source-layout-round.md 2). The constants
// stay in the groups' headers; a name two of them define differently is refused.
#pragma once

#include "game/field_c3_callees.h"
#include "game/member_sprites_callees.h"

namespace field_core::at {
using namespace ::field_c3::at;
using namespace ::member_sprites::at;
}  // namespace field_core::at
