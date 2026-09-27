// Five spell overlays of Magic_Rows, round nine group S10 (the PSX's MAGIC052,
// MAGIC053, MAGIC054, MAGIC055 and MAGIC056.EMI, rows 107, 134, 56, 65 and
// 112; read one id down, the sibling's labels Ovum, Lavaburst, Howling,
// Ebonfire and Sacrifice): 60 functions, 0x4ABCA0..0x4AE7B6, taken with the
// shared harness (magic_harness.h). docs/magic_s10.md.
#pragma once

void MagicS10_Inject();

namespace magic_s10 {

// BOF3X_SHADOW=magic_s10: the start-up fuzz, magic_s10_fuzz.cpp. Clones every
// original before MagicS10_Inject patches it.
void SelfTest();

}  // namespace magic_s10
