// World 2's areas 90, 91, 92 and 94 (the PSX's BIN/WORLD02/AREA090..094.EMI;
// area 93 has no code), compiled into the exe at 0x411F10..0x4135B0: the
// band's 45 functions - choices, handlers, area 91's six-state object with
// its glow and CLUT fade, its three-ring effect (effect kind 0x18's state
// 71), two object triggers, area 92's inline effect spawns, area 94's
// Effect_Spawn handlers and its init. Round ten group AR2C, through the area
// harness (area_harness.h). docs/area_w2c.md.
#pragma once

void AreaW2c_Inject();

namespace area_w2c {
// BOF3X_SHADOW=area_w2c: the start-up fuzz, area_w2c_fuzz.cpp - one
// area_harness::Run per area. Clones every original before AreaW2c_Inject
// patches it.
void SelfTest();
}  // namespace area_w2c
