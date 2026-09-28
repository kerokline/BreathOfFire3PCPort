// World 4's areas 175..187 (the PSX's BIN/WORLD04/AREA175..187.EMI), compiled
// into the exe at 0x4292C0..0x42A320: the band's 49 functions - areas
// 175..185's one shared set (a choice table of 31 entries, two handlers, an
// init, a cell hook and a step hook, all eleven areas' descriptors naming the
// same bodies but for choice 27, of which each area has its own byte-for-byte
// copy), area 175's own six handlers and its glide states, area 185's draw
// (EffectKind18_States[103]), area 186's init and tail kind 39, area 187's
// choice, object trigger 14 and tail kind 29, and object trigger 64. Round
// ten group AR4D, through the area harness (area_harness.h).
// docs/area_w4d.md.
#pragma once

void AreaW4d_Inject();

namespace area_w4d {
// BOF3X_SHADOW=area_w4d: the start-up fuzz, area_w4d_fuzz.cpp - one
// area_harness::Run per area. Clones every original before AreaW4d_Inject
// patches it.
void SelfTest();
}  // namespace area_w4d
