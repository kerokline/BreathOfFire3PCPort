// World 1's areas 38..41 (the PSX's BIN/WORLD01/AREA038..041.EMI), compiled
// into the exe at 0x4053B0..0x406650: the band's 48 functions - handlers,
// choices, area 40's puzzle (a mode-tail phase, an arrive hook, an init and
// the helpers they call), area 41's state machine and two object triggers.
// Round ten group AR1A, through the area harness (area_harness.h).
// docs/area_w1a.md.
#pragma once

void AreaW1a_Inject();

namespace area_w1a {
// BOF3X_SHADOW=area_w1a: the start-up fuzz, area_w1a_fuzz.cpp - one
// area_harness::Run per area. Clones every original before AreaW1a_Inject
// patches it.
void SelfTest();
}  // namespace area_w1a
