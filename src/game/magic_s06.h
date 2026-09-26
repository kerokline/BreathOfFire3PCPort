// Two spell overlays compiled into the exe (the PSX's MAGIC008.EMI and
// MAGIC020.EMI; Magic_Rows rows 42 and 8): MAGIC008's doubles of the acting
// actor - its sprite record copied into a child task - that grow, strike,
// dash and flash for nine abilities, and MAGIC020's shadows and slashes.
// Round nine group S06, taken with the shared spell harness
// (magic_harness.h). docs/magic_s06.md.
#pragma once

void MagicS06_Inject();

namespace magic_s06 {
// BOF3X_SHADOW=magic_s06: the start-up fuzz, magic_s06_fuzz.cpp. Clones every
// original before MagicS06_Inject patches it.
void SelfTest();
}  // namespace magic_s06
