// The callees constants of area_cell_hook.cpp's Area_CellHook (moved here from the .cpp so that
// the function's new file sees them: source-layout B6).
#pragma once

#include <cstdint>

#include "bof3/symbols.gen.h"

namespace area_cell_hook {
namespace at {

// 0x662F28: pairs of (area, handler) dwords. The loop runs while its cursor
// is below 0x663008 (MapCell_Handlers), so the table is 28 pairs - not the
// 100 docs/takeover-queue-areas.md counted from the bytes that follow.
constexpr ::bof3::image::Va kPairs{0x662F28};
constexpr std::uint32_t kPairStride = 8;
constexpr unsigned kPairCount = (0x663008 - kPairs.v) / kPairStride;

}  // namespace at
}  // namespace area_cell_hook
