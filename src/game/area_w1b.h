// World 1's areas 42..47 (the PSX's BIN/WORLD01/AREA042..047.EMI), compiled
// into the exe at 0x406650..0x408FE7: 55 functions (the band's 56 less
// WorldMap_DrawNeedle, round seven's). Round ten group AR1B, through the area
// harness (area_harness.h). docs/area_w1b.md.
#pragma once

void AreaW1b_Inject();

namespace area_w1b {
// BOF3X_SHADOW=area_w1b: the start-up fuzz, area_w1b_fuzz.cpp - one
// area_harness::Run per area. Clones every original before AreaW1b_Inject
// patches it.
void SelfTest();
}  // namespace area_w1b
