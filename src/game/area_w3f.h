// World 3's areas 143..146 (the PSX's BIN/WORLD03/AREA143..146.EMI),
// compiled into the exe at 0x420800..0x4223A0: the band's 53 functions -
// choices and handlers (seven shared with other areas' tables), three step
// hooks, a cell hook and area 145's tail kind 20 with its five trail
// helpers, three object triggers, two state machines in the areas' .data
// (area 145's drop, effect kinds 0xB3 and 0xB4), the glow cylinder area 143
// and area 146 each compile (area 146's copy, 0x4220D0, is called by the
// engine and by areas 36, 59, 100, 112 and 116), a CLUT shift chapter 12's
// scene calls, and a full-screen gradient (EffectKind18_States entry 96).
// Area 147 has no code in the band. Round ten group AR3F, through the area
// harness (area_harness.h). docs/area_w3f.md.
#pragma once

void AreaW3f_Inject();

namespace area_w3f {
// BOF3X_SHADOW=area_w3f: the start-up fuzz, area_w3f_fuzz.cpp - one
// area_harness::Run per area. Clones every original before AreaW3f_Inject
// patches it.
void SelfTest();
}  // namespace area_w3f
