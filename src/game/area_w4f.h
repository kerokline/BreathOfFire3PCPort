// World 4's areas 192..199 (the PSX's BIN/WORLD04/AREA192..199.EMI; 194 and
// 195 have no code here), compiled into the exe at 0x42BD60..0x42D710: the
// band's 49 functions - choices and handlers (one shared with areas 148, 167
// and 173, one with area 174, one with area 191), nine member searches over
// their own key tables, three step hooks, three tail kinds (51, 54, 57), two
// inits, area 192's talk messages chapters 14 and 15 ask for, a character
// restore, two state machines in the areas' .data (area 197's shake, effect
// kind 0xA6), an EffectKind18_States entry, and area 198's falling drops.
// Round ten group AR4F, through the area harness (area_harness.h).
// docs/area_w4f.md.
#pragma once

void AreaW4f_Inject();

namespace area_w4f {
// BOF3X_SHADOW=area_w4f: the start-up fuzz, area_w4f_fuzz.cpp - one
// area_harness::Run per area. Clones every original before AreaW4f_Inject
// patches it.
void SelfTest();
}  // namespace area_w4f
