#include "game/map_scroll.h"

#include "game/draw_pool.h"
#include "game/map_rim.h"

#include <cstdint>
#include <cstring>
#include <initializer_list>

#include "bof3/symbols.gen.h"
#include "game/map_scroll_callees.h"
#include "hook/detour.h"
#include "hook/log.h"

// The view's cell ring - its scrolling and its set-up for an area - see
// docs/map-scroll.md for each function, its PSX twin and the fuzz.
//
// MapView_CellItems is a ring of 0x38 rows by 0x1C columns, one 4-byte cell
// each (map x, map y, a draw-item word), headed by MapView_Row and
// MapView_Column: the slot at the head is the one just before view row /
// column 0. A scroll never moves a cell. It releases and refills the ring
// slots that come into view through MapView_CellToMap and moves the head and
// MapView_Origin by one map cell.

namespace map_scroll {

const Callees kOriginals = {
    DrawItemPool_ReleaseCell, MapView_CellToMap,
    Gte_SetGeomScreen, Gte_SetGeomOffset, Gte_SetBackColor, Gpu_SetPolyFT4, Gpu_SetShadeTex, AreaMap_Elevation,
    AreaMap_BakePatches, AreaMap_SetupEntries, MapView_PlaceRuns,
    AreaMap_ClutCycleStart, AreaMap_ApplyPatch,
};
Callees g = kOriginals;

}  // namespace map_scroll

void MapScroll_Inject() {
    if (bof3::WantsShadow("map_scroll")) map_scroll::SelfTest();
}
