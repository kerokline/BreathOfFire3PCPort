// The cell hook's per-area reader, 0x56E670: the second half of the
// chapter's +0x10 cell hook (0x56D7A0), which the leader's confirm button
// asks about a 0x51 cell. Round ten group ARH, taken through the area
// harness (area_harness.h). docs/area_harness.md section 8.
#pragma once

void AreaCellHook_Inject();

namespace area_cell_hook {
// BOF3X_SHADOW=area_cell_hook: the start-up fuzz, area_cell_hook_fuzz.cpp.
void SelfTest();
}  // namespace area_cell_hook
