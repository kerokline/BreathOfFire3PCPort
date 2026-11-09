// The callees constants this file's functions name as at::k...: the union of their
// groups' namespaces (tools/move_funcs.py, docs/source-layout-round.md 2). The constants
// stay in the groups' headers; a name two of them define alike (one type, one value) is
// named once by a using-declaration, one they define differently is an ambiguity the
// build reports.
#pragma once

#include "game/area_w3f_callees.h"
#include "game/area_w3g_callees.h"

namespace area_148::at {
using namespace ::area_w3f::at;
using namespace ::area_w3g::at;
using ::area_w3f::at::kCounter0;
using ::area_w3f::at::kEffectCount;
using ::area_w3f::at::kEffectStride;
using ::area_w3f::at::kMessage;
using ::area_w3f::at::kPartyStride;
using ::area_w3f::at::kTailTimer;
using ::area_w3f::at::kTextRecords;
}  // namespace area_148::at
