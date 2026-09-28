// World 2's areas 104..106 (the PSX's BIN/WORLD02/AREA104..106.EMI), compiled
// into the exe at 0x4146C0..0x4168E0: the band's 53 functions less
// WorldMapHud_BoxWait 0x414BB0 (round eight's) - area 104, the world map's
// seventh copy (WorldMap_Records record 6) with its own place hook, the leader
// controller the engine runs as leader state 12 in area 104, effect kinds
// 0x5C (the gauge companion) and 0x6A (the countdown), mode-tail kind 40, a
// minimap built at entry and an object trigger; area 105's tail, step hook
// and init; area 106's three handlers, tail kind 36 and step hook. Round ten
// group AR2E, through the area harness (area_harness.h). docs/area_w2e.md.
#pragma once

void AreaW2e_Inject();

namespace area_w2e {
// BOF3X_SHADOW=area_w2e: the start-up fuzz, area_w2e_fuzz.cpp - one
// area_harness::Run per area. Clones every original before AreaW2e_Inject
// patches it.
void SelfTest();
}  // namespace area_w2e
