// One spell overlay of Magic_Rows, round nine group S13 (the PSX's
// MAGIC063.EMI, row 143; read one id down, the sibling's label Sudden Death):
// 25 functions, 0x4B2F40..0x4B3EFE, taken with the shared harness
// (magic_harness.h). docs/magic_s13.md.
#pragma once

void MagicS13_Inject();

namespace magic_s13 {

// BOF3X_SHADOW=magic_s13: the start-up fuzz, magic_s13_fuzz.cpp. Clones every
// original before MagicS13_Inject patches it.
void SelfTest();

}  // namespace magic_s13
