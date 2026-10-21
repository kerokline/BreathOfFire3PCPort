// The narrow areas pillarboxed (DIVERGENCE DIV-0041, the wide-ring round's
// side group PB, docs/narrow-pillarbox.md).
//
// In the wide modes (BOF3X_WIDE=1 426 x 240, =2 560 x 240) an area whose map
// is narrower than the picture showed the void past its edge in the bands
// (docs/ultrawide-culls.md section 5). Such an area is now framed as a
// narrower picture on purpose: black bars drawn by us over the bands, the
// picture inside them one of the three widths the port has (320, 426, 560).
// The frame is the widest of those not wider than the area's widest screen
// row, and never below 320: an area under 426 px is framed at 320 (areas 31,
// 149, 186 and 198 on the shipped data), one from 426 to under 560 at 426
// (53, 90, 98, 153, 154, 192 and 194 - so 21:9 only), every other area not at
// all. The width is measured from the area's own map block as the game loaded
// it (AreaMap_Header), not from a table.
//
// Draw-only: the bars are appended to the frame after the game drew it, and
// nothing the game reads changes - the view, the culls, the ring, Rand are the
// wide mode's. The one other reader is Widescreen_Anchor: an element anchored
// to the wide picture's edge (DIV-0041's side message boxes, the area HUDs)
// anchors to the framed picture's edge instead, so it is not under a bar.
// Off when the view is narrow, before Widescreen_ArmFills (every self-test
// compares the original's picture), in a battle, and under BOF3X_PILLARBOX=0.
#pragma once

#include <cstdint>

#include "game/widescreen.h"

namespace pillarbox {

// The rule (docs/narrow-pillarbox.md section 2). One ring column of the
// field's view is 32 screen px at the field camera over level ground: the
// 320-wide view's widest row is 10.0 ring columns (ultrawide-culls.md 5).
constexpr unsigned kRingColumnPx = 32;

// The columns a side of the picture an area is framed at, from its widest
// screen row in ring columns: the widest of the port's three pictures (0, the
// 16:9 columns, the 21:9 columns, each by widescreen_rule::ColumnsForAspect)
// whose width the row reaches, 0 below them all.
constexpr unsigned FrameColumns(unsigned widest_row) {
    const unsigned px = widest_row * kRingColumnPx;
    const unsigned c219 = widescreen_rule::ColumnsForAspect(21, 9), c169 = widescreen_rule::ColumnsForAspect(16, 9);
    return px >= 320u + 2u * c219 ? c219 : px >= 320u + 2u * c169 ? c169 : 0u;
}
// The columns a side that stay visible in a picture of `columns` a side: the
// frame's, or the picture's own when the area is as wide as the picture.
constexpr unsigned Framed(unsigned widest_row, unsigned columns) {
    return FrameColumns(widest_row) < columns ? FrameColumns(widest_row) : columns;
}

// The widest screen row of an area map block in ring columns: along each map
// diagonal x + z (one screen row at the field's yaw 0x200, as MapView_CellToMap
// 0x56F910 walks a ring row), the span of interior cells whose tile word is not
// 0. `header` is the block (u8 width, u8 height, u16 offset, then the u16 tile
// words MapView_CellToMap reads); `bytes` how many of it may be read - a cell
// past it counts as empty.
unsigned WidestRow(const unsigned char* header, std::uint32_t bytes);

// The columns a side visible now: Widescreen_Live() unless the area being
// shown is framed narrower. 0 when the view is narrow.
unsigned VisibleColumns();

// Widescreen_Anchor's one call: the anchor `fill` (Widescreen_Fill()) pulled
// in to the framed picture's edge. `fill` itself whenever it is 0.
unsigned Anchor(unsigned fill);

// The present's one call (render_shim.cpp Present, after the frame hash): the
// bars appended to the frame being presented, when the area shown is framed.
void Cover();

// After every module's self-test (InjectAll): the rule's self-test
// (BOF3X_SHADOW=pillarbox, and '*'), then BOF3X_PILLARBOX read.
void Inject();

}  // namespace pillarbox
