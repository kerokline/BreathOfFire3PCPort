// World 4's areas 173 and 174 (the PSX's BIN/WORLD04/AREA173.EMI and
// AREA174.EMI), compiled into the exe at 0x428450..0x4292C0: the band's 39
// functions - area 173's five handlers of the band, tail kind 38, its arrive
// hook and init; area 174's twenty-one handlers, a pose helper and a two-entry
// stack dispatcher's two states; and six choices areas 175..185 share, whose
// bodies the band holds. Round ten group AR4C, through the area harness
// (area_harness.h). docs/area_w4c.md.
#pragma once

void AreaW4c_Inject();

namespace area_w4c {
// BOF3X_SHADOW=area_w4c: the start-up fuzz, area_w4c_fuzz.cpp - one
// area_harness::Run per area (173, 174, 175, and 198 for the two handlers it
// shares with 174). Clones every original before AreaW4c_Inject patches it.
void SelfTest();
}  // namespace area_w4c
