// World 4's areas 152..155, 166 and 167 (the PSX's BIN/WORLD04/AREA152..167
// .EMI; areas 156..165 have no code in the band), compiled into the exe at
// 0x4249D0..0x42655C: 48 functions. Round ten group AR4A, through the area
// harness (area_harness.h). docs/area_w4a.md.
#pragma once

void AreaW4a_Inject();

namespace area_w4a {
// BOF3X_SHADOW=area_w4a: the start-up fuzz, area_w4a_fuzz.cpp - one
// area_harness::Run per area. Clones every original before AreaW4a_Inject
// patches it.
void SelfTest();
}  // namespace area_w4a
