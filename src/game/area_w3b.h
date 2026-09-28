// World 3's areas 120 and 121 (the PSX's BIN/WORLD03/AREA120..121.EMI),
// compiled into the exe at 0x41A9D0..0x41C88C: 54 functions. Round ten group
// AR3B, through the area harness (area_harness.h). docs/area_w3b.md.
#pragma once

void AreaW3b_Inject();

namespace area_w3b {
// BOF3X_SHADOW=area_w3b: the start-up fuzz, area_w3b_fuzz.cpp - one
// area_harness::Run per area group. Clones every original before
// AreaW3b_Inject patches it.
void SelfTest();
}  // namespace area_w3b
