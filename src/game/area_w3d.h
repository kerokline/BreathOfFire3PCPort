// World 3's area 135 (the PSX's BIN/WORLD03/AREA135.EMI), compiled into the exe
// at 0x41DAD0..0x41EFE0: the band's 46 functions - four choices, twenty
// handlers (choices 4..23 are handlers 0..19), four dispatchers' nine .data
// states, a cell hook, a step hook, tail kind 18 (a 32-state machine), the
// init and eleven helpers. Round ten group AR3D, through the area harness
// (area_harness.h). docs/area_w3d.md.
#pragma once

void AreaW3d_Inject();

namespace area_w3d {
// BOF3X_SHADOW=area_w3d: the start-up fuzz, area_w3d_fuzz.cpp - one
// area_harness::Run for area 135. Clones every original before
// AreaW3d_Inject patches it.
void SelfTest();
}  // namespace area_w3d
