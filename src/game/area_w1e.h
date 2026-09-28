// World 1's areas 65 and 67 (the PSX's BIN/WORLD01/AREA065 and AREA067.EMI),
// compiled into the exe at 0x40B8C0..0x40CEEC: 49 functions. Round ten group
// AR1E, through the area harness (area_harness.h). docs/area_w1e.md.
#pragma once

void AreaW1e_Inject();

namespace area_w1e {
// BOF3X_SHADOW=area_w1e: the start-up fuzz, area_w1e_fuzz.cpp - one
// area_harness::Run per area. Clones every original before AreaW1e_Inject
// patches it.
void SelfTest();
}  // namespace area_w1e
