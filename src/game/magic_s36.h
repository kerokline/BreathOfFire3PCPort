// Three spell overlays of Magic_Rows, round nine group S36 (the PSX's
// MAGIC172, MAGIC173 and MAGIC218.EMI, rows 23, 34 and 142; read one id down,
// the sibling's labels Magic Ball, Intimidate and Aura Breath): 45 functions,
// 0x4F1E40..0x4F3535, 0x4F3540..0x4F4A53 and 0x4F52F0..0x4F59CF, taken with the
// shared harness (magic_harness.h). docs/magic_s36.md.
#pragma once

void MagicS36_Inject();

namespace magic_s36 {

// BOF3X_SHADOW=magic_s36: the start-up fuzz, magic_s36_fuzz.cpp. Clones every
// original before MagicS36_Inject patches it.
void SelfTest();

}  // namespace magic_s36
