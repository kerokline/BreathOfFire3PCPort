// World 3's areas 115..119 (the PSX's BIN/WORLD03/AREA115..119.EMI),
// compiled into the exe at 0x418BE0..0x41A9D0: the band's 56 functions not
// already ours - area 115's world-map copy (WorldMap_Records record 7, area
// 88's code over its own tables), area 116's choice, handler, step hook,
// object trigger and effect kind 0xB8, areas 117 and 118's member frames,
// rectangle searches and floor switches (one body over a table set each),
// and eighteen handlers / choices of areas 117..119 (twelve of them one
// Effect_Spawn at a party member). Round ten group AR3A, through the area
// harness (area_harness.h). docs/area_w3a.md.
#pragma once

void AreaW3a_Inject();

namespace area_w3a {
// BOF3X_SHADOW=area_w3a: the start-up fuzz, area_w3a_fuzz.cpp - one
// area_harness::Run per area. Clones every original before AreaW3a_Inject
// patches it.
void SelfTest();
}  // namespace area_w3a
