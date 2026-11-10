// The callees constants this file's functions name as at::k...: the union of their
// groups' namespaces (tools/move_funcs.py, docs/source-layout-round.md 2). The constants
// stay in the groups' headers; a name two of them define alike (one type, one value) is
// named once by a using-declaration, one they define differently is an ambiguity the
// build reports.
#pragma once

#include "game/area_w3c_callees.h"
#include "game/area_w3d_callees.h"

namespace area_134::at {
using namespace ::area_w3c::at;
using namespace ::area_w3d::at;
using ::area_w3c::at::kCounter3;
using ::area_w3c::at::kEffectCount;
using ::area_w3c::at::kEffectStride;
using ::area_w3c::at::kMessage;
using ::area_w3c::at::kPartyStride;
using ::area_w3c::at::kTextRecords;
}  // namespace area_134::at
