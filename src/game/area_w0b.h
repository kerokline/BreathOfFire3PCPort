// World 0's areas 16 and 18..26 (the PSX's BIN/WORLD00/AREA016.EMI and
// AREA018..026.EMI), compiled into the exe at 0x401B80..0x4033F2: 61
// functions. Round ten group AR0B, through the area harness (area_harness.h).
// docs/area_w0b.md.
#pragma once

void AreaW0b_Inject();

namespace area_w0b {
// BOF3X_SHADOW=area_w0b: the start-up fuzz, area_w0b_fuzz.cpp - one
// area_harness::Run per area. Clones every original before AreaW0b_Inject
// patches it.
void SelfTest();
}  // namespace area_w0b
