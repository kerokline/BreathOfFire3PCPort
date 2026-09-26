// Three spell overlays of Magic_Rows, round nine group S27 (the PSX's
// MAGIC118, MAGIC120 and MAGIC121.EMI, rows 60, 16 and 122; read one id down,
// the sibling's labels Burn, Whelp Breath and DragonBreath): 47 functions,
// 0x4DA220..0x4DD8A4, taken with the shared harness (magic_harness.h).
// docs/magic_s27.md.
#pragma once

void MagicS27_Inject();

namespace magic_s27 {

// BOF3X_SHADOW=magic_s27: the start-up fuzz, magic_s27_fuzz.cpp. Clones every
// original before MagicS27_Inject patches it.
void SelfTest();

}  // namespace magic_s27
