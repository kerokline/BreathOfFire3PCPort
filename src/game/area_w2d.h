// World 2's areas 95..100 and 103 (the PSX's BIN/WORLD02/AREA095..100 and
// AREA103.EMI), compiled into the exe at 0x4135B0..0x4146C0: the band's 53
// functions - handlers (seven shared by areas 95 and 96), choices, three
// mode-tail phases, two step hooks, five object triggers, three inits, two
// state machines in area 99's .data, effect kind 0xB7's handler and its draw
// state, and the world-map field hook for "no world map". Areas 101 and 102
// have no code in the band. Round ten group AR2D, through the area harness
// (area_harness.h). docs/area_w2d.md.
#pragma once

void AreaW2d_Inject();

namespace area_w2d {
// BOF3X_SHADOW=area_w2d: the start-up fuzz, area_w2d_fuzz.cpp - one
// area_harness::Run per area. Clones every original before AreaW2d_Inject
// patches it.
void SelfTest();
}  // namespace area_w2d
