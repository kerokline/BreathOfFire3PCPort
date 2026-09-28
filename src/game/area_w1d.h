// World 1's areas 53, 55..57 and 59..64 (the PSX's BIN/WORLD01/AREA053..064.EMI;
// areas 54 and 58 have no code), compiled into the exe at 0x40AB00..0x40B8C0:
// the band's 47 functions - choices, handlers, area 56's fall states, area
// 59's effect states, a step hook, six object triggers, a mode-tail phase,
// four inits. Round ten group AR1D, through the area harness
// (area_harness.h). docs/area_w1d.md.
#pragma once

void AreaW1d_Inject();

namespace area_w1d {
// BOF3X_SHADOW=area_w1d: the start-up fuzz, area_w1d_fuzz.cpp - one
// area_harness::Run per area. Clones every original before AreaW1d_Inject
// patches it.
void SelfTest();
}  // namespace area_w1d
