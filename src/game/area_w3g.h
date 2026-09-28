// World 3's areas 148..151 (the PSX's BIN/WORLD03/AREA148..151.EMI),
// compiled into the exe at 0x4223A0..0x4249D0: the band's 56 functions -
// area 148's handlers, choices, tail kind 31, arrive / cell hooks, object
// trigger 17 and effect kind 0x7E (a searchlight and its x87 draws), area
// 149's handler states, view shift, tail kind 33 and init, area 150's
// choices, tail kind 58 and step hook, and area 151's world-map copy
// (WorldMap_Records record 9, area 87's code over its own tables with a place
// hook of its own). Round ten group AR3G, through the area harness
// (area_harness.h). docs/area_w3g.md.
#pragma once

void AreaW3g_Inject();

namespace area_w3g {
// BOF3X_SHADOW=area_w3g: the start-up fuzz, area_w3g_fuzz.cpp - one
// area_harness::Run per area (area 148 twice). Clones every original before
// AreaW3g_Inject patches it.
void SelfTest();
}  // namespace area_w3g
