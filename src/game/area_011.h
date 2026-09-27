// Area 11's code (the PSX's BIN/WORLD00/AREA011.EMI, descriptor 0x5E2738,
// Area_Descriptors entry 11), compiled into the exe at 0x401750..0x401839:
// its two handlers and its init. Round ten group ARH, the area harness's
// proof (area_harness.h). docs/area_011.md.
#pragma once

void Area011_Inject();

namespace area_011 {
// BOF3X_SHADOW=area_011: the start-up fuzz, area_011_fuzz.cpp. Clones every
// original before Area011_Inject patches it.
void SelfTest();
}  // namespace area_011
