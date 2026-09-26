// The two spell overlays no ability loads, round nine group C3 (the PSX's
// MAGIC002 and MAGIC111.EMI, Magic_Rows rows 2 and 119): 20 functions,
// 0x499D80..0x49A7AD and 0x4D6110..0x4D67E5, taken with the shared harness
// (magic_harness.h). Row 119 is item magic (the item row table's category 0,
// index 32); row 2 is reached by no table index on the PC. docs/magic_c3.md.
#pragma once

void MagicC3_Inject();

namespace magic_c3 {

// BOF3X_SHADOW=magic_c3: the start-up fuzz, magic_c3_fuzz.cpp. Clones every
// original before MagicC3_Inject patches it.
void SelfTest();

}  // namespace magic_c3
