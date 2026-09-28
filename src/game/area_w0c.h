// World 0's areas 27..29 and 32..37 (the PSX's BIN/WORLD00/AREA027..037.EMI),
// compiled into the exe at 0x403400..0x4053B0: the band's 52 functions not
// already ours - handlers, choices, mode-tail phases, a step hook, object
// triggers and the state handlers the areas' .data tables reach. Round ten
// group AR0C, through the area harness (area_harness.h). docs/area_w0c.md.
#pragma once

void AreaW0c_Inject();

namespace area_w0c {
// BOF3X_SHADOW=area_w0c: the start-up fuzz, area_w0c_fuzz.cpp - one
// area_harness::Run per area. Clones every original before AreaW0c_Inject
// patches it.
void SelfTest();
}  // namespace area_w0c
