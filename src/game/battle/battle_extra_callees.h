// The callees constants this file's functions name as at::k...: the union of their
// groups' namespaces (tools/move_funcs.py, docs/source-layout-round.md 2). The constants
// stay in the groups' headers; a name two of them define alike (one type, one value) is
// named once by a using-declaration, one they define differently is an ambiguity the
// build reports.
#pragma once

#include "game/battle_e1_callees.h"
#include "game/rest_3a_callees.h"

namespace battle_extra::at {
using namespace ::battle_e1::at;
using namespace ::rest_3a::at;
using ::battle_e1::at::kActor;
using ::battle_e1::at::kCharSize;
using ::battle_e1::at::kFlags;
using ::battle_e1::at::kMemberSize;
}  // namespace battle_extra::at
