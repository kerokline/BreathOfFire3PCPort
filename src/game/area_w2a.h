// World 2's areas 76..82 and 84 (the PSX's BIN/WORLD02/AREA076..084.EMI;
// area 83 has no code in the band), compiled into the exe at
// 0x40EB90..0x40F720: the band's 50 functions - choices, handlers, a step
// and a cell hook, an object trigger, area 77's leap step and effect helper,
// area 79's object state and its slide, and the camera reset twelve areas'
// tables name. Round ten group AR2A, through the area harness
// (area_harness.h). docs/area_w2a.md.
#pragma once

void AreaW2a_Inject();

namespace area_w2a {
// BOF3X_SHADOW=area_w2a: the start-up fuzz, area_w2a_fuzz.cpp - one
// area_harness::Run per area. Clones every original before AreaW2a_Inject
// patches it.
void SelfTest();
}  // namespace area_w2a
