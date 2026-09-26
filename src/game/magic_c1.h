// Six spell overlays of Magic_Rows, round nine group C1 - the unfinished
// skills TCRF lists (docs/cut-content.md section 2): the PSX's MAGIC010,
// MAGIC080, MAGIC113, MAGIC145, MAGIC146 and MAGIC213.EMI, rows 9, 27, 10,
// 149, 150 and 148: 62 functions, taken with the shared harness
// (magic_harness.h). docs/magic_c1.md.
#pragma once

void MagicC1_Inject();

namespace magic_c1 {

// BOF3X_SHADOW=magic_c1: the start-up fuzz, magic_c1_fuzz.cpp. Clones every
// original before MagicC1_Inject patches it.
void SelfTest();

}  // namespace magic_c1
