// Four enemy-skill overlays of Magic_Rows, round nine group C2 (the PSX's
// MAGIC057, MAGIC081, MAGIC116 and MAGIC129.EMI, rows 84, 85, 59 and 145;
// read one id down, TCRF's Bone Dance, RottenBreath, UtmostAttack and
// Holocaust): 64 functions, taken with the shared harness (magic_harness.h).
// docs/magic_c2.md.
#pragma once

void MagicC2_Inject();

namespace magic_c2 {

// BOF3X_SHADOW=magic_c2: the start-up fuzz, magic_c2_fuzz.cpp. Clones every
// original before MagicC2_Inject patches it.
void SelfTest();

}  // namespace magic_c2
