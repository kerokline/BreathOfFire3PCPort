// World 2's areas 108 and 110..113 (the PSX's BIN/WORLD02/AREA108,
// AREA110..113.EMI), compiled into the exe at 0x4168E0..0x418BE0: the band's
// 53 functions - handlers and choices, two cell hooks, a step hook and an
// arrive hook, three mode-tail phases, two inits, three object triggers, the
// area's own state tables (a fade, an effect kind's two states), area 111's
// 7 x 7 block grid and its helpers, area 113's reward. Area 109 has no code.
// Round ten group AR2F, through the area harness (area_harness.h).
// docs/area_w2f.md.
#pragma once

void AreaW2f_Inject();

namespace area_w2f {
// BOF3X_SHADOW=area_w2f: the start-up fuzz, area_w2f_fuzz.cpp - one
// area_harness::Run per area. Clones every original before AreaW2f_Inject
// patches it.
void SelfTest();
}  // namespace area_w2f
