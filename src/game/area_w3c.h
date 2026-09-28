// World 3's areas 124..125, 127..128 and 130..134 (the PSX's
// BIN/WORLD03/AREA124..134.EMI; areas 126 and 129 name no code), compiled into
// the exe at 0x41C890..0x41DAD0: the band's 56 functions - two inits of area
// 72's random placement, handlers (party-list Effect_Spawns, effects at the
// running object, camera shifts), choices, three mode-tail phases and the
// shared tail disarm the engine calls too, three object triggers, a patch
// init, and an EffectKind18 state that draws a screen-wide gradient. Round ten
// group AR3C, through the area harness (area_harness.h). docs/area_w3c.md.
#pragma once

void AreaW3c_Inject();

namespace area_w3c {
// BOF3X_SHADOW=area_w3c: the start-up fuzz, area_w3c_fuzz.cpp - one
// area_harness::Run per area. Clones every original before AreaW3c_Inject
// patches it.
void SelfTest();
}  // namespace area_w3c
