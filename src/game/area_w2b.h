// World 2's areas 85..88 (the PSX's BIN/WORLD02/AREA085..088.EMI), compiled
// into the exe at 0x40F720..0x411EFF: 66 functions (the band's 68 less
// WorldMap_PinSprite and WorldMap_FrameWait, rounds seven's and eight's).
// Round ten group AR2B, through the area harness (area_harness.h).
// docs/area_w2b.md.
#pragma once

void AreaW2b_Inject();

namespace area_w2b {
// BOF3X_SHADOW=area_w2b: the start-up fuzz, area_w2b_fuzz.cpp - one
// area_harness::Run per area. Clones every original before AreaW2b_Inject
// patches it.
void SelfTest();
}  // namespace area_w2b
