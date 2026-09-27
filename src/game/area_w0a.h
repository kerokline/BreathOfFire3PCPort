// World 0's first area overlays: areas 0..5, 7, 8, 10, 12, 13 and 15 (the
// PSX's BIN/WORLD00/AREA000..015.EMI, less 6, 9, 11 and 14), compiled into
// the exe at 0x401000..0x401B80 - 49 functions of choice handlers, script
// handlers, one init, one object trigger and area 15's two state tables.
// Area 11's three, in the same band, are area_011.cpp's (round ten wave
// one). Round ten group AR0A, through the area harness (area_harness.h).
// docs/area_w0a.md.
#pragma once

void AreaW0a_Inject();

namespace area_w0a {
// BOF3X_SHADOW=area_w0a: the start-up fuzz, area_w0a_fuzz.cpp - one Run per
// area under the one shadow name. Clones every original before
// AreaW0a_Inject patches it.
void SelfTest();
}  // namespace area_w0a
