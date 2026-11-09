// The callees constants this file's functions name as at::k...: the union of their
// groups' namespaces (tools/move_funcs.py, docs/source-layout-round.md 2). The constants
// stay in the groups' headers; a name two of them define alike (one type, one value) is
// named once by a using-declaration, one they define differently is an ambiguity the
// build reports.
#pragma once

#include "game/area_w1d_callees.h"
#include "game/area_w1e_callees.h"

namespace area_65::at {
using namespace ::area_w1d::at;
using namespace ::area_w1e::at;
using ::area_w1d::at::kEffectStride;
using ::area_w1d::at::kMessage;
using ::area_w1d::at::kTextRecords;
}  // namespace area_65::at
