// World 1's areas 48..52 (the PSX's BIN/WORLD01/AREA048..052.EMI), compiled
// into the exe at 0x408FF0..0x40AB00: the band's 57 functions - choices,
// handlers, two mode-tail phases, a step, an arrive and two cell hooks, an
// object trigger, an init, area 49's effect frame and area 52's block-moving
// helpers. Round ten group AR1C, through the area harness (area_harness.h).
// docs/area_w1c.md.
#pragma once

void AreaW1c_Inject();

namespace area_w1c {
// BOF3X_SHADOW=area_w1c: the start-up fuzz, area_w1c_fuzz.cpp - one
// area_harness::Run per area. Clones every original before AreaW1c_Inject
// patches it.
void SelfTest();
}  // namespace area_w1c
