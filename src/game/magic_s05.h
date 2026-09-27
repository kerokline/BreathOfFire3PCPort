// Two spell units of Magic_Rows, round nine group S05 (the PSX's MAGIC017
// and MAGIC018/019.EMI, rows 43, 5, 6, 53, 54 and 68): 29 functions,
// 0x4A11E0..0x4A218F, taken with the shared harness (magic_harness.h). Each
// row copies the acting actor's record into one to six child tasks (the
// images), which MAGIC008's code (rows 5, 6, 53, 54, 68) or MAGIC017's own
// (row 43) moves. docs/magic_s05.md.
#pragma once

void MagicS05_Inject();

namespace magic_s05 {

// BOF3X_SHADOW=magic_s05: the start-up fuzz, magic_s05_fuzz.cpp. Clones every
// original before MagicS05_Inject patches it.
void SelfTest();

}  // namespace magic_s05
