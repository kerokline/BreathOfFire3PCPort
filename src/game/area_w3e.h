// World 3's areas 136 and 139..142 (the PSX's BIN/WORLD03/AREA136 and
// AREA139..142.EMI), compiled into the exe at 0x41EFE0..0x420800: the band's
// 52 functions - choices and handlers (area 136's camera shift shared with
// areas 119, 131 and 188), three mode-tail phases, three cell hooks, a step
// hook, two object triggers, two inits, the state tables of areas 141 and 142,
// effect kind 0x71's handler and its three states, area 141's cell-run
// painter, and five object placers chapter 14's code calls. Areas 137 and 138
// have no code. Round ten group AR3E, through the area harness
// (area_harness.h). docs/area_w3e.md.
#pragma once

void AreaW3e_Inject();

namespace area_w3e {
// BOF3X_SHADOW=area_w3e: the start-up fuzz, area_w3e_fuzz.cpp - one
// area_harness::Run per area. Clones every original before AreaW3e_Inject
// patches it.
void SelfTest();
}  // namespace area_w3e
