// Four spell overlays of Magic_Rows, round nine group S22 (the PSX's
// MAGIC096..MAGIC099.EMI, rows 102, 19, 37 and 36; read one id down, the
// sibling's labels Blizzard, Jolt, Lightning and Myollnir): 56 functions,
// 0x4C8D40..0x4CC96E, taken with the shared harness (magic_harness.h).
// docs/magic_s22.md.
#pragma once

void MagicS22_Inject();

namespace magic_s22 {

// BOF3X_SHADOW=magic_s22: the start-up fuzz, magic_s22_fuzz.cpp. Clones every
// original before MagicS22_Inject patches it.
void SelfTest();

}  // namespace magic_s22
