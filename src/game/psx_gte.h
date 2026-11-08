// The port's PSX library layer, GTE half: the integer functions among
// originals 0x5A7B90..0x5A94BE (docs/psx-library-layer.md).
#pragma once

#include <cstdint>

void PsxGte_Inject();

namespace psx_gte {
extern std::uint32_t g_lv_exact;   // defined in psx_gte.cpp (named here for a moved reader: source-layout B0)
}  // namespace psx_gte
