// The callees constants this file's functions name as at::k...: the union of their
// groups' namespaces (tools/move_funcs.py, docs/source-layout-round.md 2). The constants
// stay in the groups' headers; a name two of them define alike (one type, one value) is
// named once by a using-declaration, one they define differently is an ambiguity the
// build reports.
#pragma once

#include "game/area_w1f_callees.h"
#include "game/area_w2a_callees.h"

namespace area_75::at {
using namespace ::area_w1f::at;
using namespace ::area_w2a::at;
using ::area_w1f::at::kCounter0;
using ::area_w1f::at::kCounter3;
using ::area_w1f::at::kEffectStride;
using ::area_w1f::at::kMessage;
using ::area_w1f::at::kPartyStride;
}  // namespace area_75::at
