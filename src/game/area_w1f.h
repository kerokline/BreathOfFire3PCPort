// World 1's areas 68..69 and 71..75 (the PSX's BIN/WORLD01/AREA068..075.EMI),
// compiled into the exe at 0x40CEF0..0x40EB90: the band's 59 functions -
// choices, handlers and their .data state tables, three object triggers, two
// inits, a step hook, and area 75's tail kind 30 with its press phases, their
// helpers and the counters' draw (area 42's window draw among them). Round ten
// group AR1F, through the area harness (area_harness.h). docs/area_w1f.md.
#pragma once

void AreaW1f_Inject();

namespace area_w1f {
// BOF3X_SHADOW=area_w1f: the start-up fuzz, area_w1f_fuzz.cpp - one
// area_harness::Run per area. Clones every original before AreaW1f_Inject
// patches it.
void SelfTest();
}  // namespace area_w1f
