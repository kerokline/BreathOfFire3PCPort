// original 0x56E670 (PSX 0x801A9CB4), Area_CellHook: the per-area table at 0x662F28 (group
// ART names it and its layout) read for Game_AreaNumber. docs/area_harness.md
// section 8. A faithful replacement: no DIVERGENCE.md entry is owed.
#include "game/area_cell_hook.h"

#include <cstdint>

#include "bof3/symbols.gen.h"
#include "game/area_cell_hook_callees.h"
#include "game/area_harness.h"
#include "game/move_script_bytes.h"
#include "hook/detour.h"
#include "hook/log.h"

void AreaCellHook_Inject() {
    if (bof3::WantsShadow("area_cell_hook")) area_cell_hook::SelfTest();
}
