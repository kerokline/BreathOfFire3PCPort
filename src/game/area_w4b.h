// World 4's areas 168..172 (the PSX's BIN/WORLD04/AREA168..172.EMI), compiled
// into the exe at 0x426560..0x428450: the band's 56 functions - choices,
// handlers, three inits, five mode-tail phases, three step hooks, two arrive
// hooks, four object triggers, two member frames the engine calls with their
// rectangle searches, two state machines in area 172's .data, effect kind
// 0xA5's handler with its three states and two draws. Round ten group AR4B,
// through the area harness (area_harness.h). docs/area_w4b.md.
#pragma once

void AreaW4b_Inject();

namespace area_w4b {
// BOF3X_SHADOW=area_w4b: the start-up fuzz, area_w4b_fuzz.cpp - one
// area_harness::Run per area. Clones every original before AreaW4b_Inject
// patches it.
void SelfTest();
}  // namespace area_w4b
