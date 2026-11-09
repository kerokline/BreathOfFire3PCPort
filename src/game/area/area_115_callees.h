// The callees constants this file's functions name as at::k...: the union of their
// groups' namespaces (tools/move_funcs.py, docs/source-layout-round.md 2). The constants
// stay in the groups' headers; a name two of them define alike (one type, one value) is
// named once by a using-declaration, one they define differently is an ambiguity the
// build reports.
#pragma once

#include "game/area_w2f_callees.h"
#include "game/area_w3a_callees.h"

namespace area_115::at {
using namespace ::area_w2f::at;
using namespace ::area_w3a::at;
using ::area_w2f::at::kCounter0;
using ::area_w2f::at::kMessage;
using ::area_w2f::at::kPartyStride;
}  // namespace area_115::at
